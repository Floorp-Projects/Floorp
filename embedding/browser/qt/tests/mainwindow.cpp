#include "mainwindow.h"

#include <qlineedit.h>
#include <qaction.h>
#include <qmenubar.h>
#include <qtoolbar.h>
#include <qfiledialog.h>
#include <qstatusbar.h>
#include <qlayout.h>

#include "qgeckoembed.h"

const QString rsrcPath = ":/images/lin";

MyMainWindow::MyMainWindow()
{
    QFrame *box = new QFrame(this);
    qecko = new QGeckoEmbed(box, "qgecko");
    qecko->resize(500, 700); // FIXME
    box->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    setCentralWidget( box );

    QToolBar *toolbar = new QToolBar(this);
    toolbar->setWindowTitle("Location:");

    QAction *action = new QAction(QIcon(rsrcPath + "/back.png"), tr( "Go Back"), toolbar);
    action->setShortcut(Qt::ControlModifier + Qt::Key_B);
//                         toolbar, "goback");
    connect(action, SIGNAL(activated()), this, SLOT(goBack()));
    toolbar->addAction(action);

    action = new QAction(QIcon(rsrcPath + "/forward.png" ), tr( "Go Forward"), toolbar);
    action->setShortcut(Qt::ControlModifier + Qt::Key_F);
//                         toolbar, "goforward");
    connect(action, SIGNAL(activated()), this, SLOT(goForward()));
    toolbar->addAction(action);

    action = new QAction(QIcon(rsrcPath + "/stop.png" ), tr("Stop"), toolbar);
    action->setShortcut(Qt::ControlModifier + Qt::Key_S);
//                         toolbar, "stop");
    connect(action, SIGNAL(activated()), this, SLOT(stop()));
    toolbar->addAction(action);

    location = new QLineEdit(toolbar);
    toolbar->addWidget(location);

    QMenu *menu = new QMenu(tr( "&File" ), this);
    menuBar()->addMenu( menu );

    QAction *a = new QAction( QIcon(rsrcPath + "/fileopen.png" ), tr( "&Open..." ), toolbar);
    a->setShortcut( Qt::ControlModifier + Qt::Key_O );
//                              toolbar, "fileOpen" );
    connect( a, SIGNAL( activated() ), this, SLOT( fileOpen() ) );
    //a->addTo( toolbar );
    menu->addAction(a);


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
    QString fn = QFileDialog::getOpenFileName( this, tr( "HTML-Files (*.htm *.html);;All Files (*)" ), QDir::currentPath());
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
    qDebug("progress %d / %d (%s)",  current, max, url.toUtf8().data());
}

void MyMainWindow::slotProgress(int current, int max)
{
    qDebug("progress %d / %d ", current, max);
}
