#include "myimage.h"
#include <QPainter>
#include <QBitmap>
#include <QDir>
#include <QDebug>
#include <QPixmapCache>

#if(QT_VERSION>=0x050000)
MyImage::MyImage(QQuickItem *parent) :
    QQuickPaintedItem(parent)
#else
MyImage::MyImage(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
#endif
{
#if(QT_VERSION<0x050000)
    setFlag(QGraphicsItem::ItemHasNoContents, false);
#endif
    m_status = Null;
    m_cache = true;
    m_grayscale = false;
    scalingFactor = 0;
    isSetWidth = false;
    isSetHeight = false;
    m_source = "";
    m_sourceSize = QSize(-1,-1);

    connect(&manager, SIGNAL(finished(QNetworkReply*)), SLOT(onDownImageFinished(QNetworkReply*)));

    bitmap = new QBitmap;
    connect(this, SIGNAL(smoothChanged(bool)), SLOT(reLoad()));

    connect(this, SIGNAL(widthChanged()), SLOT(onWidthChanged()));
    connect(this, SIGNAL(heightChanged()), SLOT(onHeightChanged()));
}

QUrl MyImage::source() const
{
    return m_source;
}

QUrl MyImage::maskSource() const
{
    return m_maskSource;
}

bool MyImage::cache() const
{
    return m_cache;
}

bool MyImage::grayscale() const
{
    return m_grayscale;
}

QImage MyImage::chromaticToGrayscale(const QImage &image)
{
    if(image.isGrayscale ())
        return image;
    QImage iGray(image.size (), QImage::Format_ARGB32_Premultiplied);
    for(int i=0;i<image.width ();++i){
        for(int j=0;j<image.height ();++j){
            QRgb pixel = image.pixel(i,j);
            pixel = qGray (pixel);
            pixel = qRgb(pixel,pixel,pixel);            
            iGray.setPixel (i,j,pixel);
        }
    }
    return iGray;
}

QString MyImage::imageFormatToString(const QByteArray &array)
{
    QByteArray str = array.toHex ();

        if(str.mid (2,6)=="504e47")
            return "png";
        if(str.mid (12,8)=="4a464946")
            return "jpg";
        if(str.left (6)=="474946")
            return "gif";
        if(str.left (4)=="424d")
            return "bmp";
        return "";
}

void MyImage::downloadImage(const QUrl &url)
{
    setStatus(Loading);

    if(reply!=NULL){
        reply->abort();
        //先结束上次的网络请求
    }

    QNetworkRequest request;
    request.setUrl(url);
    reply = manager.get(request);
}

void MyImage::handlePixmap()
{
    if(m_maskSource.toString()!=""&&(!bitmap->isNull())){
        int max_width = bitmap->width();
        int max_height = bitmap->height();
        max_width = pixmap.width()>max_width?pixmap.width():max_width;
        max_height = pixmap.height()>max_height?pixmap.height():max_height;

        pixmap = pixmap.scaled(max_width, max_height);
        pixmap.setMask (bitmap->scaled(max_width, max_height));
    }
    if(smooth())
        pixmap = pixmap.scaled(boundingRect().size().toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    else
        pixmap = pixmap.scaled(boundingRect().size().toSize(), Qt::IgnoreAspectRatio);

    update();
}

void MyImage::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
#if(QT_VERSION>=0x050000)
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
#else
    QDeclarativeItem::geometryChanged(newGeometry, oldGeometry);
#endif
    reLoad();
}

void MyImage::setImage(QImage &image)
{
    if(image.isNull())
        return;

    setDefaultSize(image.size());
    if(m_sourceSize==QSize(-1,-1)){
        m_sourceSize = image.size();
    }else{
        image = image.scaled(m_sourceSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    QPixmapCache::insert(m_source.toString(), QPixmap::fromImage(image));

    scalingFactor = image.width()/image.height();

    if(!isSetWidth){
        if(!isSetHeight){
            setImplicitWidth(image.width());//设置默认大小
            setImplicitHeight(image.height());
        }else{
            setImplicitWidth(boundingRect().height()*scalingFactor);
        }
    }else{
        setImplicitHeight(boundingRect().width()/scalingFactor);
    }

    if(m_grayscale){//如果为黑白
        image = chromaticToGrayscale(image);//转换为黑白图
    }
    pixmap = QPixmap::fromImage(image);
    handlePixmap();
    setStatus(Ready);
}

void MyImage::onDownImageFinished(QNetworkReply *reply)
{
    if(reply->error() == QNetworkReply::NoError){
        //qDebug()<<QString::fromUtf8("MyImage:图片下载成功");

        QImage image;
        if( !image.loadFromData(reply->readAll())){
            emit loadError ();
            setStatus(Error);
            //qDebug()<<QString::fromUtf8("MyImage:图片加载出错");
            return;
        }
        setImage(image);
    }else{
        setStatus(Error);
        emit loadError();
    }
}

void MyImage::onWidthChanged()
{
    isSetWidth = true;
    setImplicitHeight(boundingRect().width()/scalingFactor);
}

void MyImage::onHeightChanged()
{
    isSetHeight = true;
    setImplicitWidth(boundingRect().height()*scalingFactor);
}

#if(QT_VERSION>=0x050000)
void MyImage::paint(QPainter *painter)
#else
void MyImage::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
#endif
{
    painter->drawPixmap (0, 0, pixmap);
}

MyImage::State MyImage::status() const
{
    return m_status;
}

QSize MyImage::sourceSize() const
{
    return m_sourceSize;
}

QSize MyImage::defaultSize() const
{
    return m_defaultSize;
}

void MyImage::setSource(QUrl arg)
{
    if (!m_cache||m_source != arg) {
        setStatus(Loading);
        m_source = arg;
        reLoad();
        //加载图片
        if(m_source != arg)
            emit sourceChanged(arg);
    }
}

void MyImage::setMaskSource(QUrl arg)
{
    if (m_maskSource != arg) {
        m_maskSource = arg;
        QString str = arg.toString ();
        if( str.mid (0, 3) == "qrc")
            str = str.mid (3, str.count ()-3);
        bitmap->load (str);
        reLoad();
        emit maskSourceChanged(arg);
    }
}

void MyImage::setCache(bool arg)
{
    if (m_cache != arg) {
        m_cache = arg;
        emit cacheChanged(arg);
    }
}

void MyImage::setGrayscale(bool arg)
{
    if(m_grayscale!=arg){
        m_grayscale = arg;
        reLoad();
        emit grayscaleChanged(arg);
    }
}

void MyImage::setStatus(MyImage::State arg)
{
    if (m_status != arg) {
        m_status = arg;
        emit statusChanged(arg);
    }
}

void MyImage::reLoad()
{
    QString str = m_source.toString();
    if(str=="")
        return;

    if(m_cache){
        QPixmap *temp_pximap = QPixmapCache::find(str);
        if(temp_pximap!=NULL){//如果缓存区已经有图片
            pixmap = *temp_pximap;
            handlePixmap();
            setStatus(Ready);
            return;
        }
    }

    if(str.indexOf("http")==0){//如果是网络图片
        downloadImage(m_source);
        return;
    }

    if( str.mid (0, 3) == "qrc")
        str = str.mid (3, str.count ()-3);

    QImage image;
    if( !image.load (str)){
        emit loadError ();
        setStatus(Error);
        return;
    }
    setImage(image);
}

void MyImage::setSourceSize(QSize arg)
{
    if (m_sourceSize != arg) {
        m_sourceSize = arg;
        QPixmapCache::remove(m_source.toString());
        reLoad();
        emit sourceSizeChanged(arg);
    }
}

void MyImage::setDefaultSize(QSize arg)
{
    if (m_defaultSize != arg) {
        m_defaultSize = arg;
        emit defaultSizeChanged(arg);
    }
}
