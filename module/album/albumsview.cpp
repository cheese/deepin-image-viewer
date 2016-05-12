#include "albumsview.h"
#include "albumdelegate.h"
#include "controller/signalmanager.h"
#include "controller/popupmenumanager.h"
#include <QDebug>
#include <QBuffer>
#include <QJsonDocument>
#include <QMouseEvent>

namespace {

const QString MY_FAVORITES_ALBUM = "My favorites";
const QString RECENT_IMPORTED_ALBUM = "Recent imported";
const int ITEM_SPACING = 58;
const QSize ITEM_DEFAULT_SIZE = QSize(152, 168);

}  // namespace

AlbumsView::AlbumsView(QWidget *parent)
    : QListView(parent),
      m_dbManager(DatabaseManager::instance()),
      m_popupMenu(new PopupMenuManager(this))
{
    setMouseTracking(true);
    AlbumDelegate *delegate = new AlbumDelegate(this);
    connect(delegate, &AlbumDelegate::editingFinished,
            this, [=](const QModelIndex &index) {
        closePersistentEditor(index);
    });
    setItemDelegate(delegate);
    m_itemModel = new QStandardItemModel(this);
    setModel(m_itemModel);

    setContextMenuPolicy(Qt::CustomContextMenu);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setResizeMode(QListView::Adjust);
    setViewMode(QListView::IconMode);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setUniformItemSizes(true);
    setSpacing(ITEM_SPACING);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setDragEnabled(false);

    // Aways has Favorites album
    m_dbManager->insertImageIntoAlbum("My favorites", "", "");

    connect(this, &AlbumsView::doubleClicked,
            this, &AlbumsView::onDoubleClicked);
    connect(this, &AlbumsView::customContextMenuRequested,
            this, [=] (const QPoint &pos) {
        m_popupMenu->showMenu(createMenuContent(indexAt(pos)));
    });
    connect(m_popupMenu, &PopupMenuManager::menuItemClicked,
            this, &AlbumsView::onMenuItemClicked);
}

QModelIndex AlbumsView::addAlbum(const DatabaseManager::AlbumInfo &info)
{
    // AlbumName ImageCount BeginTime EndTime Thumbnail
    QStringList imgNames = m_dbManager->getImageNamesByAlbum(info.name);
    if (imgNames.isEmpty()) {
        return QModelIndex();
    }
    QByteArray thumbnailByteArray;
    QBuffer inBuffer( &thumbnailByteArray );
    inBuffer.open( QIODevice::WriteOnly );
    // write inPixmap into inByteArray
    QString imageName = imgNames.first();
    if (info.name != "My favorites" && imageName.isEmpty()) {
        for (QString name : imgNames) {
            if (! name.isEmpty()) {
                imageName = name;
                break;
            }
        }
    }
    if ( ! m_dbManager->getImageInfoByName(imageName)
         .thumbnail.save( &inBuffer, "JPG" )) {
        qDebug() << "Write pixmap to buffer error!" << info.name;
    }

    QVariantList datas;
    datas.append(QVariant(info.name));
    datas.append(QVariant(info.count));
    datas.append(QVariant(info.beginTime));
    datas.append(QVariant(info.endTime));
    datas.append(QVariant(thumbnailByteArray));

    QStandardItem *item = new QStandardItem();
    QList<QStandardItem *> items;
    items.append(item);
    m_itemModel->appendRow(items);

    QModelIndex index = m_itemModel->index(m_itemModel->rowCount() - 1, 0, QModelIndex());
//    m_itemModel->setData(index, QVariant(info.name), Qt::EditRole);
    m_itemModel->setData(index, QVariant(datas), Qt::DisplayRole);
    m_itemModel->setData(index, QVariant(ITEM_DEFAULT_SIZE), Qt::SizeHintRole);

    return index;
}

QSize AlbumsView::itemSize() const
{
    return m_itemSize;
}

void AlbumsView::setItemSize(const QSize &itemSize)
{
    m_itemSize = itemSize;
    for (int column = 0; column < m_itemModel->columnCount(); column ++) {
        QModelIndex index = m_itemModel->index(0, column, QModelIndex());
        m_itemModel->setData(index, QVariant(itemSize), Qt::SizeHintRole);
    }
}

void AlbumsView::mousePressEvent(QMouseEvent *e)
{
    if (! indexAt(e->pos()).isValid()) {
        this->selectionModel()->clearSelection();
    }

    QListView::mousePressEvent(e);
}

QString AlbumsView::getAlbumName(const QModelIndex &index)
{
    QString albumName = "";
    QList<QVariant> datas = index.model()->data(index, Qt::DisplayRole).toList();
    if (! datas.isEmpty()) {
        albumName = datas[0].toString();
    }

    return albumName;
}

QString AlbumsView::getNewAlbumName() const
{
    const QString nan = "New Album";
    const QStringList albums = m_dbManager->getAlbumNameList();
    QStringList tmpList;
    for (QString album : albums) {
        if (album.startsWith("New Album")) {
            tmpList << album;
        }
    }

    if (tmpList.isEmpty()) {
        return nan;
    }
    else {
        qSort(tmpList.begin(), tmpList.end());
        for (int i = tmpList.length() - 1; i > 0; i ++) {
            const int count
                    = QString(QString(tmpList.at(i)).split(nan).last()).toInt();
            if (count >= 0) {
                return nan + QString::number(i + 1);
            }
        }

        return nan;
    }
}

QString AlbumsView::createMenuContent(const QModelIndex &index)
{
    QJsonArray items;
    if (index.isValid()) {
        bool isSpecial = false;
        QList<QVariant> datas = index.model()->data(index, Qt::DisplayRole).toList();
        if (! datas.isEmpty()) {
            const QString albumName = datas[0].toString();
            if (albumName == MY_FAVORITES_ALBUM
                    || albumName == RECENT_IMPORTED_ALBUM) {
                isSpecial = true;
            }
        }

        items.append(createMenuItem(IdSeparator, "", true));
        items.append(createMenuItem(IdView, tr("View")));
        items.append(createMenuItem(IdStartSlideShow, tr("Start slide show"), false, "Ctrl+Alt+P"));
        items.append(createMenuItem(IdSeparator, "", true));
        items.append(createMenuItem(IdCopy, tr("Copy"), false, "Ctrl+C"));
        if (! isSpecial)
            items.append(createMenuItem(IdDelete, tr("Delete"), false, "Ctrl+Delete"));
        items.append(createMenuItem(IdSeparator, "", true));
        items.append(createMenuItem(IdExport, tr("Export")));
        items.append(createMenuItem(IdAlbumInfo, tr("Album info"), false, "Ctrl+I"));
    }
    else {
        items.append(createMenuItem(IdCreate, tr("Create Album")));
    }

    QJsonObject contentObj;
    contentObj["x"] = 0;
    contentObj["y"] = 0;
    contentObj["items"] = QJsonValue(items);

    QJsonDocument document(contentObj);

    return QString(document.toJson());
}

QJsonValue AlbumsView::createMenuItem(const MenuItemId id,
                                      const QString &text,
                                      const bool isSeparator,
                                      const QString &shortcut,
                                      const QJsonObject &subMenu)
{
    return QJsonValue(m_popupMenu->createItemObj(id,
                                                                  text,
                                                                  isSeparator,
                                                                  shortcut,
                                                                  subMenu));
}

void AlbumsView::onMenuItemClicked(int menuId)
{
    QModelIndex index;
    const QString newAlbumName = getNewAlbumName();
    const QString currentAlbumName = getAlbumName(currentIndex());
    switch (MenuItemId(menuId)) {
    case IdCreate:
        m_dbManager->insertImageIntoAlbum(newAlbumName, "", "");
        index = addAlbum(m_dbManager->getAlbumInfo(newAlbumName));
        openPersistentEditor(index);
        break;
    case IdView:
        emit openAlbum(getAlbumName(currentIndex()));
        break;
    case IdStartSlideShow:
        break;
    case IdCopy:
        break;
    case IdDelete:
        if (currentAlbumName != "My favorites"
                && currentAlbumName != "Recent imported") {
            m_dbManager->removeAlbum(getAlbumName(currentIndex()));
            m_itemModel->removeRow(currentIndex().row());
        }
        break;
    case IdAlbumInfo:
        break;
    default:
        break;
    }
}

void AlbumsView::onDoubleClicked(const QModelIndex &index)
{
    emit openAlbum(getAlbumName(index));
}
