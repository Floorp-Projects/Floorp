/* $Id: toolbars.cpp,v 1.1 1998/09/25 18:01:41 ramiro%netscape.com Exp $
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Arnt
 * Gulbrandsen.  Further developed by Warwick Allison, Kalle Dalheimer,
 * Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and
 * Paul Olav Tvete.  Copyright (C) 1998 Warwick Allison, Kalle
 * Dalheimer, Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard
 * Nord and Paul Olav Tvete.  All Rights Reserved.
 */

#include "toolbars.h"
#include "QtContext.h"
#include "QtBookmarkButton.h"

#include "icons.h"
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qcombo.h>
#include <qmovie.h>
#include <qpainter.h>
#include <qtooltip.h>
#include <qstatusbar.h>
#include <qwidgetstack.h>
#include <qprogbar.h>
#include <qapp.h>

#include <fe_proto.h>


RCSTAG("$Id: toolbars.cpp,v 1.1 1998/09/25 18:01:41 ramiro%netscape.com Exp $");


Toolbars::Toolbars( QtContext* context, QMainWindow * parent )
    : QObject( parent, 0 )
{
    navigation = new QToolBar( "Navigation Toolbar", parent,
			       QMainWindow::Top, TRUE );
    location = new QToolBar( "Navigation Toolbar", parent,
			     QMainWindow::Top, TRUE );

#if 0
    personal = new QToolBar( "Navigation Toolbar", parent,
			     QMainWindow::Top, TRUE );
#else
    personal = 0;
#endif

    QToolTipGroup * ttg = parent->toolTipGroup();

    QToolButton * b;

    back = new QToolButton( Icon( "TB_Back" ), "Back",
			    0, context,
			    SLOT(cmdBack()), navigation, "back" );
    back->setFocusPolicy( QWidget::NoFocus );
    // do back menu here
    forward = new QToolButton( Icon( "TB_Forward" ), "Forward",
			       0, context, SLOT(cmdForward()),
			       navigation, "forward" );
    forward->setFocusPolicy( QWidget::NoFocus );
    // do forward menu here

    b = new QToolButton( Icon( "TB_Reload" ), "Reload",
			 0, context,
			 SLOT(cmdReload()), navigation, "reload" );
    b->setFocusPolicy( QWidget::NoFocus );
    b = new QToolButton( Icon( "TB_Home" ), "Home",
			 0, context,
			 SLOT(cmdHome()), navigation, "home" );
    b->setFocusPolicy( QWidget::NoFocus );

    b = new QToolButton( Icon( "TB_Search" ), "Search",
			 0, context,
			 SLOT(cmdSearch()), navigation, "search" ); // #### ?
    b->setFocusPolicy( QWidget::NoFocus );

    b = new QToolButton( Icon( "TB_Places" ), "Places",
			 0, context,
			 SLOT(cmdGuide()), navigation, "places" );
    b->setFocusPolicy( QWidget::NoFocus );

#ifdef EDITOR
    b = new QToolButton( Icon( "TB_EditPage" ), "Edit Page",
			 0, context, SLOT(cmdEditPage()),
			 navigation, "edit page" );
    b->setFocusPolicy( QWidget::NoFocus );
#endif

    images = new QToolButton( Icon( "TB_LoadImages" ), "Show Images",
			      0, context, SLOT(cmdShowImages()),
			      navigation, "show images" );
    images->setFocusPolicy( QWidget::NoFocus );
    b = new QToolButton( Icon( "TB_Print" ), "Print",
			 0, context,
			 SLOT(cmdPrint()), navigation, "print" );
    b->setFocusPolicy( QWidget::NoFocus );
    b = new QToolButton( Icon( "TB_MixSecurity" ), "View Security",
			 0, context, SLOT(cmdViewSecurity()),
			 navigation, "view security" );
    b->setFocusPolicy( QWidget::NoFocus );
    stop = new QToolButton( Icon( "TB_Stop" ), "Stop",
			    0, context, SLOT(cmdStopLoading()),
			    navigation, "stop loading" );
    stop->setFocusPolicy( QWidget::NoFocus );

    // hole
    QWidget * hole = new QWidget( navigation, "stretchable hole" );
    navigation->setStretchableWidget( hole );

    animation = new MovieToolButton( navigation,
				     Movie( "biganim" ), Pixmap( "notanim" ) );
    animation->setFocusPolicy( QWidget::NoFocus );
    QToolTip::add( animation, "Mozilla", ttg,
		   "Visit Mozilla's home away from home" );
    connect( animation, SIGNAL(clicked()), this, SLOT(visitMozilla()) );

    // location bar
    b = new QtBookmarkButton( Pixmap("BM_FolderO"), "Bookmarks",
			      context, location );
    b->setFocusPolicy( QWidget::NoFocus );
    QLabel * l = new QLabel( "Bookmarks", location );
    l->setFixedSize( l->sizeHint() );
    l->installEventFilter( b );
    l = new QLabel( "  ", location );
    l->setFixedSize( l->sizeHint() );
    l = new QLabel( "Location:", location );
    l->setFixedSize( l->sizeHint() );
    url = new QComboBox( TRUE, location );
    url->setAutoCompletion( TRUE );
    url->insertItem( "http://www.troll.no" );
    url->setCurrentItem( 0 );
    connect( url, SIGNAL(activated(const char *)),
	     context, SLOT(browserGetURL(const char *)) );
    url->setMaxCount( 100 );
    url->setSizeLimit( 15 );
    url->setInsertionPolicy( QComboBox::NoInsertion );
    connect( this, SIGNAL(openURL(const char *)),
	     context, SLOT(browserGetURL(const char *)) );

    location->setStretchableWidget( url );

#if 0

    // personal bar
    b = new QToolButton( personal );
    b->setText( "Filler" );
    b->setFocusPolicy( QWidget::NoFocus );

    // hole
    hole = new QWidget( personal, "stretchable hole" );
    personal->setStretchableWidget( hole );

#endif

    // now, confusingly, do the status bar too.  it's strange, but...

    // security label
    status = parent->statusBar();
    security = new QLabel( status, "security info" );
    security->setPixmap( Pixmap( "Dash_Unsecure" ) );
    security->setFixedWidth( security->sizeHint().width() );
    status->addWidget( security, 0, FALSE );

    // progress bar(s)
    progressBars = new QWidgetStack( status );
    status->addWidget( progressBars, 10, FALSE );

    knownProgress = new QProgressBar( progressBars, "known progress" );
    knownProgress->setFrameStyle( QFrame::NoFrame );
    unknownProgress = new UnknownProgress( progressBars, "unknown progress" );

    // "Document: done" and other long-time messages
    messages = new QLabel( status, "long-time messages" );
    status->addWidget( messages, 50, FALSE );
    messages->setText( "Ready to surf the web" );

    t = new QTimer( this );
    connect( t, SIGNAL(timeout()),
	     this, SLOT(hideProgressBars()) );
    t2 = new QTimer( this );
    connect( t2, SIGNAL(timeout()),
	     animation, SLOT(start()) );

    connect( ttg, SIGNAL(showTip(const char *)),
	      status, SLOT(message(const char *)) );
    connect( ttg, SIGNAL(removeTip()), status, SLOT(clear()) );

    url->setFocus();
}


Toolbars::~Toolbars()
{
    // nothing
}


MovieToolButton::MovieToolButton( QToolBar * parent,
				  const QMovie & movie,
				  const Pixmap & pixmap )
    : QToolButton( parent, 0 )
{
    m = new QMovie( movie );
    m->pause();
    m->connectResize(this, SLOT(movieResized(const QSize&)));
    m->connectUpdate(this, SLOT(movieUpdated(const QRect&)));
    pm = pixmap;
}


MovieToolButton::~MovieToolButton()
{
    delete m;
}


void MovieToolButton::drawButtonLabel( QPainter * p )
{
    if ( m->running() ) {
	QPixmap fpm = m->framePixmap();
	p->drawPixmap( width()/2-fpm.width()/2, height()/2-fpm.height()/2,
		       fpm );
    } else {
	p->fillRect( 1, 1, width()-2, height()-2, white );
	p->drawPixmap( width()/2-pm.width()/2, height()/2-pm.height()/2,
		       pm );
    }
}


void MovieToolButton::movieUpdated(const QRect& r)
{
    int x = width()/2-m->framePixmap().width()/2;
    int y = height()/2-m->framePixmap().height()/2;
    repaint( x, y, r.width(), r.height(), FALSE );
}


void MovieToolButton::movieResized(const QSize&)
{
    repaint( FALSE );
}


void MovieToolButton::start()
{
    m->unpause();
    repaint( FALSE );
}


void MovieToolButton::stop()
{
    m->pause();
    repaint( FALSE );
}


QSize MovieToolButton::sizeHint() const
{
    return QSize( 42,42 );
}


void Toolbars::visitMozilla()
{
    emit openURL( "http://www.troll.no/qtmozilla/" );
}


void Toolbars::setBackButtonEnabled( bool e )
{
    back->setEnabled( e );
}


void Toolbars::setForwardButtonEnabled( bool e )
{
    forward->setEnabled( e );
}


void Toolbars::setStopButtonEnabled( bool e )
{
    stop->setEnabled( e );
}


void Toolbars::setLoadImagesButtonEnabled( bool e )
{
    images->setEnabled( e );
}


UnknownProgress::UnknownProgress( QWidget * parent, const char * name )
    : QWidget( parent, name )
{
    x = 0;
    t = new QTimer( this );
    connect( t, SIGNAL(timeout()),
	     this, SLOT(progress()) );
}


void UnknownProgress::progress()
{
    if ( !t->isActive() )
	t->start( 50, FALSE );

    if ( !d )
	d = 2;

    if ( d > 0 && x + 4*d > width() ) {
	int ox = x;
	x = width() - d;
	d = -d;
	repaint( ox, 0, width() - ox, height(), FALSE );
    } else if ( d > 0 ) {
	x += d;
	repaint( x-d, 0, 4*d, height(), FALSE );
    } else if ( d < 0 && x < -d ) {
	int ox = x - 3*x;
	d = -d;
	x = 0;
	repaint( 0, 0, QMAX(ox, 3*d), height(), FALSE );
    } else if ( d < 0 ) {
	x += d;
	repaint( x, 0, 4*d, height(), FALSE );
    }
}


void UnknownProgress::paintEvent( QPaintEvent * e )
{
    QPainter p( this );
    p.eraseRect( e->rect() );
    if ( d )
	p.fillRect( x, 0, 3*d, height(), QBrush( colorGroup().foreground() ) );
}


void UnknownProgress::done()
{
    t->stop();
    d = 0;
    repaint();
}


void UnknownProgress::resizeEvent( QResizeEvent * e )
{
    if ( d < 0 )
	d = -d;
    x = 0;
    QWidget::resizeEvent( e );
}




void Toolbars::setupProgress( int )
{
    knownProgress->reset();
    knownProgress->setTotalSteps( 100 );
    unknownProgress->progress();
    progressBars->raiseWidget( unknownProgress );
    progressAsPercent = FALSE;
    t->stop();
    t2->start( 20, TRUE );
}


void Toolbars::signalProgress( int percent )
{
    if ( percent > 99 ) {
	endProgress();
	return;
    }
    t->stop();
    if ( !t2->isActive() )
	t2->start( 20, TRUE );
    if ( !progressAsPercent ) {
	if ( percent > 80 )
	    return;
	progressBars->raiseWidget( knownProgress );
	progressAsPercent = TRUE;
	unknownProgress->done();
    }
    knownProgress->setProgress( percent );
}


void Toolbars::signalProgress()
{
    t->stop();
    if ( !t2->isActive() )
	t2->start( 20, TRUE );
    if ( !progressAsPercent )
	unknownProgress->progress();
}


void Toolbars::endProgress()
{
    knownProgress->setProgress( 100 );
    progressAsPercent = FALSE;
    t->start( 300, TRUE );
    t2->stop();
}


void Toolbars::setMessage( const char * m )
{
    messages->setText( m );
    status->clear();
}


void Toolbars::hideProgressBars()
{
    animation->stop();
    unknownProgress->done();
    progressBars->raiseWidget( unknownProgress );
    messages->setText( "" );
}


void Toolbars::setComboText( const char * newURL )
{
    if ( !newURL )
	return;
    char * u = qstrdup( newURL );
    char * p = strchr( u, '#' );
    if ( p )
	*p = 0;
    int i = url->count();
    while( --i > -1 ) {
	if ( !strcmp( u, url->text( i ) ) )
	    url->removeItem( i );
    }
    url->insertItem( u, 0 );
    url->setCurrentItem( 0 );
    delete[] u;
}
