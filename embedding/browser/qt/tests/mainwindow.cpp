#include "mainwindow.h"

#include <qaction.h>
#include <qmenubar.h>
#include <qtoolbar.h>
#include <qfiledialog.h>
#include <qstatusbar.h>
#include <qhbox.h>

#include "qgeckoembed.h"


MyMainWindow::MyMainWindow()
{
    QHBox *box = new QHBox(this);
    qecko = new QGeckoEmbed(box, "qgecko");
    box->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    setCentralWidget( box );

    //QToolBar *toolbar = new QToolBar(this);

    QPopupMenu *menu = new QPopupMenu(this);
    menuBar()->insertItem( tr( "&File" ), menu );

    QAction *a = new QAction( QPixmap::fromMimeSource( "filenew.xpm" ), tr( "&Open..." ), CTRL + Key_O, this, "fileOpen" );
    connect( a, SIGNAL( activated() ), this, SLOT( fileOpen() ) );
    //a->addTo( toolbar );
    a->addTo( menu );


    connect( qecko, SIGNAL(linkMessage(const QString &)),
             statusBar(), SLOT(message(const QString &)) );
    connect( qecko, SIGNAL(jsStatusMessage(const QString &)),
             statusBar(), SLOT(message(const QString &)) );
    connect( qecko, SIGNAL(windowTitleChanged(const QString &)),
             SLOT(setCaption(const QString &)) );
    connect( qecko, SIGNAL(startURIOpen(const QString &, bool &)),
             SLOT(startURIOpen(const QString &, bool &)) );

}

void MyMainWindow::fileOpen()
{
    QString fn = QFileDialog::getOpenFileName( QString::null, tr( "HTML-Files (*.htm *.html);;All Files (*)" ), this );
    if ( !fn.isEmpty() )
	qecko->loadURL( fn );
}

void MyMainWindow::startURIOpen(const QString &, bool &)
{
    qDebug("XX in the signal");
}
