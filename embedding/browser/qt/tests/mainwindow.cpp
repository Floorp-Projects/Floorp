#include "mainwindow.h"

#include <qlineedit.h>
#include <qaction.h>
#include <qmenubar.h>
#include <qtoolbar.h>
#include <qfiledialog.h>
#include <qstatusbar.h>
#include <qvbox.h>
#include <qlayout.h>

#include "qgeckoembed.h"


MyMainWindow::MyMainWindow()
{
    QVBox *box = new QVBox(this);
    qecko = new QGeckoEmbed(box, "qgecko");
    box->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    setCentralWidget( box );

    QToolBar *toolbar = new QToolBar(this);
    toolbar->setLabel("Location:");

    QAction *action = new QAction(QPixmap::fromMimeSource( "back.png" ), tr( "Go Back"), CTRL + Key_B,
                         toolbar, "goback");
    connect(action, SIGNAL(activated()), this, SLOT(goBack()));
    action->addTo(toolbar);

    action = new QAction(QPixmap::fromMimeSource( "forward.png" ), tr( "Go Forward"), CTRL + Key_F,
                         toolbar, "goforward");
    connect(action, SIGNAL(activated()), this, SLOT(goForward()));
    action->addTo(toolbar);

    action = new QAction(QPixmap::fromMimeSource( "stop.png" ), tr("Stop"), CTRL + Key_S,
                         toolbar, "stop");
    connect(action, SIGNAL(activated()), this, SLOT(stopLoad()));
    action->addTo(toolbar);

    location = new QLineEdit(toolbar);
    toolbar->setStretchableWidget(location);

    QPopupMenu *menu = new QPopupMenu(this);
    menuBar()->insertItem( tr( "&File" ), menu );

    QAction *a = new QAction( QPixmap::fromMimeSource( "fileopen.png" ), tr( "&Open..." ), CTRL + Key_O,
                              toolbar, "fileOpen" );
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
    connect(qecko, SIGNAL(locationChanged(const QString&)),
            location, SLOT(setText(const QString&)));
    connect(qecko, SIGNAL(progress(int, int)),
            SLOT(slotProgress(int, int)));
    connect(qecko, SIGNAL(progressAll(const QString&, int, int)),
            SLOT(slotProgress(const QString&, int, int)));


    connect( location, SIGNAL(returnPressed()), SLOT(changeLocation()));

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

void MyMainWindow::changeLocation()
{
    qecko->loadURL( location->text() );
}

void MyMainWindow::goBack()
{
    qecko->goBack();
}

void MyMainWindow::goForward()
{
    qecko->goForward();
}

void MyMainWindow::stop()
{
    qecko->stopLoad();
}

void MyMainWindow::slotProgress(const QString &url, int current, int max)
{
    qDebug("progress %d / %d (%s)",  current, max, url.latin1());
}

void MyMainWindow::slotProgress(int current, int max)
{
    qDebug("progress %d / %d ", current, max);
}
