/* $Id: QtBrowserContext.cpp,v 1.1 1998/09/25 18:01:28 ramiro%netscape.com Exp $
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
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include "QtBrowserContext.h"
#include "OpenDialog.h"
#include "FindDialog.h"
#include "SecurityDialog.h"
#include "toolbars.h"
#include "DialogPool.h"
#include "menus.h"

#include <qmainwindow.h>
#include <qstatusbar.h>
#include <qobjcoll.h>
#include <qmsgbox.h>
#include <qtimer.h>
#include <qscrollview.h>
#include <qapp.h>
#include <qmenubar.h>
#include <qkeycode.h>
#include <qcursor.h>
#include <qbitmap.h>
#include <qaccel.h>
#include <qclipboard.h>

#include "layers.h"

#include "libimg.h"             /* Image Library public API. */
#include "libevent.h"             /* Image Library public API. */
#include "il_util.h"            /* Colormap/Colorspace API. */
#include "il_icons.h"           /* Image icon enumeration. */
#include "prtypes.h"

#include "prefapi.h"

#include "QtPrefs.h"

#include "contextmenu.h"

//#include "QtHistoryContext.h"


static unsigned char handmask_bits[] = {
  0xfe,0x01,0x00,0x00,
  0xff,0x03,0x00,0x00,
  0xff,0x07,0x00,0x00,
  0xff,0x0f,0x00,0x00,
  0xfe,0x1f,0x00,0x00,
  0xf8,0x1f,0x00,0x00,
  0xfc,0x1f,0x00,0x00,
  0xf8,0x3f,0x00,0x00,
  0xfc,0x7f,0x00,0x00,
  0xf8,0xff,0x00,0x00,
  0xf0,0x7f,0x00,0x00,
  0xe0,0x3f,0x00,0x00,
  0xc0,0x1f,0x00,0x00,
  0x80,0x0f,0x00,0x00,
  0x00,0x07,0x00,0x00,
  0x00,0x02,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00
};

static unsigned char hand_bits[] = {
  0x00,0x00,0x00,0x00,
  0xfe,0x01,0x00,0x00,
  0x01,0x02,0x00,0x00,
  0x7e,0x04,0x00,0x00,
  0x08,0x08,0x00,0x00,
  0x70,0x08,0x00,0x00,
  0x08,0x08,0x00,0x00,
  0x70,0x14,0x00,0x00,
  0x08,0x22,0x00,0x00,
  0x30,0x41,0x00,0x00,
  0xc0,0x20,0x00,0x00,
  0x40,0x12,0x00,0x00,
  0x80,0x08,0x00,0x00,
  0x00,0x05,0x00,0x00,
  0x00,0x02,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00
};

static QCursor *handcursor = 0;

static void unimplemented( const char*, ... )
{
    // debug( string );
}

 class QtBrowserScrollView : public QScrollView {
    QtContext* context;
public:
    QtBrowserScrollView(QtContext* cx, QWidget* parent, const char* name) :
	QScrollView(parent, name, WPaintClever),
	context(cx)
    {
	setResizePolicy( Manual );
	viewport()->setFocusProxy( this );
	viewport()->setFocusPolicy( ClickFocus );
    }

    virtual void viewportPaintEvent( QPaintEvent* pe )
    {
	QPainter* p = context->painter();
	// Some compositor code doesn't like width or height being 1.
	QRect r = pe->rect();
	//if ( r.width() < 2 ) r.setWidth(2);
	//if ( r.height() < 2 ) r.setHeight(2);

	p->setClipRect(r);
	int cx = r.x() + contentsX();
	int cy = r.y() + contentsY();
	int cw = r.width();
	int ch = r.height();
	context->updateRect( cx, cy, cw, ch );
	context->clearPainter(); // or should we save/restore?
    }

    virtual void keyPressEvent( QKeyEvent* );

    QPainter* makePainter()
    {
	return new QPainter(viewport());
    }

    QWidget* contentWidget()
    {
	return viewport();
    }
};


void QtBrowserScrollView::keyPressEvent( QKeyEvent* e )
{
    switch( e->key() ) {
    case Key_Left:
	scrollBy( -fontMetrics().width('m'), 0 );
	break;
    case Key_Right:
	scrollBy( fontMetrics().width('m'), 0 );
	break;
    default:
	e->ignore();
	break;
    }
}
	


QtBrowserContext::QtBrowserContext(MWContext *cx,
				   Chrome *decor, QWidget* parent,
				   const char* name, WFlags f,
				   int _scrolling_policy, int _bw)
    : QtContext(cx)
{
    unimplemented( "QtBrowserContext::QtBrowserContext, not using Chrome %p", decor );
    browserWidget = new QMainWindow( parent, name, f );
    if ( !parent ) {
	// use -geometry, etc. as if we are the main widget
	QWidget* desktop = QApplication::desktop();
	browserWidget->resize(
	    QMIN(600, desktop->width()*4/5),
	    QMIN(700, desktop->height()*4/5)
	);
	qApp->setMainWidget(browserWidget);
	// but there isn't one - multiple browsers all equal.
	qApp->setMainWidget(0);
	browserWidget->setUsesBigPixmaps( TRUE );
	browserWidget->setRightJustification( TRUE );
	populateMenuBar( browserWidget->menuBar() );
	toolbars = new Toolbars( this, browserWidget );
	connect( this, SIGNAL(messengerMessage(const char *, int)),
		 browserWidget->statusBar(),
		 SLOT(message(const char *, int)) );
	connect( this, SIGNAL(messengerMessageClear()),
		 browserWidget->statusBar(), SLOT(clear()) );
	QAccel * accel = new QAccel( browserWidget );
	// up, down, page-up, page-down
	accel->connectItem( accel->insertItem( Key_Up ),
			    this, SLOT(scrollUp()) );
	accel->connectItem( accel->insertItem( Key_Down ),
			    this, SLOT(scrollDown()) );
	accel->connectItem( accel->insertItem( Key_PageUp ),
			    this, SLOT(pageUp()) );
	accel->connectItem( accel->insertItem( Key_PageDown ),
			    this, SLOT(pageDown()) );
	// ctrl/alt right, left
	accel->connectItem( accel->insertItem( CTRL+Key_Left ),
			    this, SLOT(cmdBack()) );
	accel->connectItem( accel->insertItem( ALT+Key_Left ),
			    this, SLOT(cmdBack()) );
	accel->connectItem( accel->insertItem( CTRL+Key_Right ),
			    this, SLOT(cmdForward()) );
	accel->connectItem( accel->insertItem( ALT+Key_Right ),
			    this, SLOT(cmdForward()) );
    } else {
	toolbars = 0;
    }
    messageFlickerTimer = new QTimer( this );
    connect( messageFlickerTimer, SIGNAL(timeout()),
	     this, SIGNAL(messengerMessageClear()) );
    contextMenu = 0;

    scrollView = new QtBrowserScrollView( this, browserWidget, "content" );
    // set the scrollbar policiy. Default is AUTO
    scrolling_policy = _scrolling_policy;
    if ( scrolling_policy == LO_SCROLL_NO ||
	 scrolling_policy == LO_SCROLL_NEVER){
	//      fprintf(stderr, "SCROLLBAR: nope \n");
      scrollView->setHScrollBarMode(QScrollView::AlwaysOff);
      scrollView->setVScrollBarMode(QScrollView::AlwaysOff);
    } else if ( scrolling_policy == LO_SCROLL_YES ){
	//      fprintf(stderr, "SCROLLBAR: yes \n");
      scrollView->setHScrollBarMode(QScrollView::AlwaysOn);
      scrollView->setVScrollBarMode(QScrollView::AlwaysOn);
    }

    bw = _bw;

    scrollView->contentWidget()->installEventFilter( this );
    browserWidget->installEventFilter( this );
    scrollView->contentWidget()->setMouseTracking(true);
    connect(scrollView, SIGNAL(contentsMoving(int,int)),
	    this, SLOT(viewMoved(int,int)));
    browserWidget->setCentralWidget( scrollView );
    browserWidget->show();
    refreshURLTimer = 0;
    initImageCallbacks();
}

QtBrowserContext::~QtBrowserContext()
{
    clearPainter();

    LO_DiscardDocument (mwContext());

    /* Destroy the compositor associated with the context. */
    if (mwContext()->compositor)
      {
  	CL_DestroyCompositor(mwContext()->compositor);
  	mwContext()->compositor = 0;
      }

    /* clean the context somewhat. Probabaly incomplete */
    IL_RemoveGroupObserver(mwContext()->img_cx, imageGroupObserver,
  			   (void *)mwContext());
    IL_DestroyGroupContext(mwContext()->img_cx);
    mwContext()->img_cx = 0;

    delete topLevelWidget();
}


QPainter* QtBrowserContext::makePainter()
{
    return scrollView->makePainter();
}

QWidget *QtBrowserContext::contentWidget() const
{
    return scrollView->contentWidget();
}

QWidget *QtBrowserContext::topLevelWidget() const
{
    return browserWidget;
}

int QtBrowserContext::documentWidth() const
{
  return scrollView->contentsWidth();
}

int QtBrowserContext::documentHeight() const
{
    return scrollView->contentsHeight();
}

int QtBrowserContext::visibleWidth() const
{
  return scrollView->width()-16;
}

int QtBrowserContext::visibleHeight() const
{
    return scrollView->height();
}

int QtBrowserContext::documentXOffset() const
{
    return scrollView->contentsX();
}

int QtBrowserContext::documentYOffset() const
{
    return scrollView->contentsY();
}

int QtBrowserContext::scrollWidth() const
{
    return scrollView->width();
}

int QtBrowserContext::scrollHeight() const
{
    return scrollView->height();
}

void QtBrowserContext::documentSetContentsPos( int x, int y )
{
    scrollView->setContentsPos( x, y );
}

bool QtBrowserContext::eventFilter( QObject *o, QEvent *e )
{
    if (o==scrollView->contentWidget()){
	switch (e->type()) {
	case Event_MouseButtonPress:
	    if ( ((QMouseEvent*)e)->button() == RightButton
		&& mwContext()->compositor )
	    {
		CL_Event event;
		event.fe_event_size = 0;
		event.fe_event = 0;
		event.type = CL_EVENT_MOUSE_BUTTON_DOWN;
		event.which = 3;
		event.x = ((QMouseEvent*) e)->pos().x() + documentXOffset();
		event.y = ((QMouseEvent*) e)->pos().y() + documentYOffset();
		CL_DispatchEvent(mwContext()->compositor, &event);
	    }
	    break;
	case Event_MouseButtonRelease:
	    if ( ((QMouseEvent*)e)->button() == LeftButton
		&& mwContext()->compositor )
	    {
		CL_Event event;
		event.fe_event_size = 0;
		event.fe_event = 0;
		event.type = CL_EVENT_MOUSE_BUTTON_UP;
		event.which = 1;
		event.x = ((QMouseEvent*) e)->pos().x() + documentXOffset();
		event.y = ((QMouseEvent*) e)->pos().y() + documentYOffset();
		CL_DispatchEvent(mwContext()->compositor, &event);
	    }
	
	    break;
	case Event_MouseMove:
	    if ( mwContext()->compositor )
	    {
		CL_Event event;
		event.fe_event_size = 0;
		event.fe_event = 0;
		event.type = CL_EVENT_MOUSE_MOVE;
		//#warning Matthias: think about drag and drop
		event.which = 0; // a lie
		event.x = ((QMouseEvent*) e)->pos().x() + documentXOffset();
		event.y = ((QMouseEvent*) e)->pos().y() + documentYOffset();
		CL_DispatchEvent(mwContext()->compositor, &event);
	    }
	    break;
	case Event_Resize:
	    adjustCompositorSize();
	    break;
	default:
	    break;
	}
    } else if (o==topLevelWidget()){
	switch (e->type()) {
	case Event_Resize:
	    {
	      MWContext *top_context = XP_GetNonGridContext( mwContext() );
	      if (top_context == mwContext()){
		if (top_context->url){
		  XP_InterruptContext(top_context);
		  URL_Struct *url =
		    SHIST_CreateURLStructFromHistoryEntry( top_context,
							   SHIST_GetCurrent( &(top_context->hist ) ));
#if 0//defined DEBUG_paul
    debug( "##### resize-> NET_GetURL" );
#endif

		  if (url)
		    NET_GetURL( url, FO_CACHE_AND_PRESENT, top_context, url_exit);}
		
	      }
	    }
	    break;
	default:
	    break;
	}
    }
    return FALSE;
}

void QtBrowserContext::viewMoved(int, int)
{
    adjustCompositorPosition();
}


/* Returns the URL string of the LO_Element, if it has one.
   Returns "" for LO_ATTR_ISFORM, which are a total kludge...
 */
static char *
qtfe_url_of_xref (MWContext *context, LO_Element *xref, long x, long y)
{
  switch (xref->type)
    {
    case LO_TEXT:
      if (xref->lo_text.anchor_href)
	{
	  return (char *) xref->lo_text.anchor_href->anchor;
	}
      else
	{
          return (char *) NULL;
	}

    case LO_IMAGE:
      if (xref->lo_image.is_icon &&
          xref->lo_image.icon_number == IL_IMAGE_DELAYED)
        {
          long width, height;
	  width = height = 40; //###########
	  //          fe_IconSize(IL_IMAGE_DELAYED, &width, &height);
	  unimplemented( "please implement fe_IconSize()" );
          if (xref->lo_image.alt &&
              xref->lo_image.alt_len &&
              (x > xref->lo_image.x + xref->lo_image.x_offset + 1 + 4 +
                  width))
            {
              if (xref->lo_image.anchor_href)
                {
                  return (char *) xref->lo_image.anchor_href->anchor;
                }
              else
                {
                  return (char *) NULL;
                }
            }
          else
            {
              return (char *) xref->lo_image.image_url;
            }
        }
      else if (xref->lo_image.image_attr->attrmask & LO_ATTR_ISFORM)
        {
          return "";
        }
      /*
       * This would be a client-side usemap image.
       */
      else if (xref->lo_image.image_attr->usemap_name != NULL)
        {
          LO_AnchorData *anchor_href;

          long ix = xref->lo_image.x + xref->lo_image.x_offset;
          long iy = xref->lo_image.y + xref->lo_image.y_offset;
          long mx = x - ix - xref->lo_image.border_width;
          long my = y - iy - xref->lo_image.border_width;

          anchor_href = LO_MapXYToAreaAnchor(context, (LO_ImageStruct *)xref,
							mx, my);
          if (anchor_href)
            {
              if (anchor_href->alt)
                {
                  return (char *) anchor_href->alt;
                }
              else
                {
                  return (char *) anchor_href->anchor;
                }
            }
          else
            {
              return (char *) NULL;
            }
        }
      else
        {
          if (xref->lo_image.anchor_href)
            {
              return (char *) xref->lo_image.anchor_href->anchor;
            }
          else
            {
              return (char *) NULL;
            }
        }

    default:
      return 0;
    }
}

extern void // in forms.cpp
submitForm(MWContext *context, LO_Element *element,
                              int32 /*event*/, void * /*closure*/,
                              ETEventStatus status);


bool QtBrowserContext::handleLayerEvent(CL_Layer *layer,
					CL_Event *layer_event)
{
    switch (layer_event->type){
    case CL_EVENT_MOUSE_BUTTON_DOWN:
	if ( layer_event->which == 3 ) {
	    if ( contextMenu )
		delete contextMenu;

	    LO_Element* le = LO_XYToElement(mwContext(),
					    layer_event->x,
					    layer_event->y,
					    layer);
	    QString linkToURL, imageURL;
	    if ( le && le->type == LO_TEXT ) {
		LO_AnchorData* ad = le->lo_text.anchor_href;
		if ( ad )
		    linkToURL = (char *)(ad->anchor);
	    } else if ( le && le->type == LO_IMAGE ) {
		LO_AnchorData* ad = le->lo_image.anchor_href;
		if ( ad )
		    linkToURL = (char *)(ad->anchor);
		imageURL = (char *)(le->lo_image.image_url);
	    }
	
	    QPoint p( layer_event->x - documentXOffset(),
		      layer_event->y - documentYOffset() );
	    p = scrollView->contentWidget()->mapToGlobal( p );
	    contextMenu = new ContextMenu( this, p, linkToURL, imageURL );
	    connect( contextMenu, SIGNAL(destroyed()),
		     this, SLOT(contextMenuDied()) );
	    contextMenu->show();
	}
	break;
    case CL_EVENT_MOUSE_BUTTON_UP:
	{
	    long x, y;
	    x = layer_event->x;
	    y = layer_event->y;
	
	    LO_Element* le = LO_XYToElement(mwContext(), x, y, layer);
#if 0// defined DEBUG_ettrich || defined DEBUG_paul
	    if (le){
		printf("QtBrowserContext::handleLayerEvent we have a le of type = %d\n", le->type);
	    }
	    else {
		printf("QtBrowserContext::handleLayerEvent no le \n");
	    }
#endif
	
	    if ( le ) {
		char* anchor = qtfe_url_of_xref( mwContext(), le, x, y );
		long mx = -1;
		long my = -1;
	
		if (anchor) {
		    if ( le->type == LO_IMAGE ) {
		        long ix = le->lo_image.x + le->lo_image.x_offset;
		        long iy = le->lo_image.y + le->lo_image.y_offset;
		        mx = x - ix - le->lo_image.border_width;
		        my = y - iy - le->lo_image.border_width;
		        if ( le->lo_image.image_attr->attrmask & LO_ATTR_ISFORM ) {
			    QPoint p(mx,my);
			    submitForm( mwContext(), le, 0, &p, EVENT_OK );
			    goto nogeturl;
			} else if ( le->lo_image.image_attr->usemap_name != NULL ) {
			    LO_AnchorData *anchor_href =
				LO_MapXYToAreaAnchor(mwContext(),
                                             (LO_ImageStruct *)le, mx, my);
			    if ( anchor_href && anchor_href->anchor )
				anchor = (char *) anchor_href->anchor;
			} else if ( le->lo_image.image_attr->attrmask & LO_ATTR_ISMAP ) {
			    if ( le->lo_image.anchor_href )
				anchor = (char *) le->lo_image.anchor_href->anchor;
			}
		    }

		    {
			URL_Struct * url = NET_CreateURLStruct (anchor, NET_DONT_RELOAD);
			if ( mx >= 0 && my >= 0 )
			    NET_AddCoordinatesToURLStruct (url, mx, my );
			MWContext *top_context = XP_GetNonGridContext( mwContext() );
			qt(top_context)->getURL(url);
		    }
		nogeturl:
		    ;
		}
	    }
	}
	break;
    case CL_EVENT_MOUSE_MOVE:
	{
	    long x, y;
	    x = layer_event->x;
	    y = layer_event->y;
	    LO_Element* le = LO_XYToElement(mwContext(), x, y, layer);
	    char* anchor = 0;
	    if ( le )
		anchor = qtfe_url_of_xref( mwContext(), le, x, y );

	    setupToolbarSignals();
	    if ( anchor && currentMessage == anchor ) {
		// nothing
	    } else if ( anchor ) {
		if (!handcursor)
		    handcursor = new QCursor(QBitmap(32,32,hand_bits,TRUE),
					     QBitmap(32,32,handmask_bits,TRUE),
					     0,2);
		contentWidget()->setCursor(*handcursor);
		setMessage( anchor );
	    } else if ( !currentMessage.isEmpty() ) {
		contentWidget()->setCursor(ArrowCursor);
		messageFlickerTimer->start( 150, TRUE );
		currentMessage = 0;
	    }
	}
    default:
	return false;
	break;
    }
    return true;
}


void QtBrowserContext::setDocDimension(int /*iLocation*/, int32 iWidth, int32 iLength)
{
    scrollView->resizeContents( iWidth, iLength );
}

void QtBrowserContext::setDocPosition(int /*iLocation*/, int32 x, int32 y)
{
    scrollView->setContentsPos(x,y);
}


void imageGroupObserver(XP_Observable observable, XP_ObservableMsg message,
			   void *message_data, void *closure)
{
    //IL_GroupMessageData *data = (IL_GroupMessageData*)message_data;
    MWContext *context = (MWContext *)closure;

#if defined DEBUG_ettrich
    printf("ImageGroupObserver**********************\n");
    switch (message) {
    case IL_STARTED_LOADING:
	printf("IL_STARTED_LOADING \n");
	break;

    case IL_ABORTED_LOADING:
	printf("IL_ABORTED_LOADING \n");
	break;

    case IL_FINISHED_LOADING:
        /* Note: This means that all images currently in the context have
           finished loading.  If network activity has not ceased, then
           new images could start to load in the image context. */
	printf("IL_FINISHED_LOADING \n");
	break;

    case IL_STARTED_LOOPING:
	printf("IL_STARTED_LOOPING \n");
        break;

    case IL_FINISHED_LOOPING:
        /* Note: This means that all loaded images currently in the context
           have finished looping.  If network activity has not ceased, then
           new images could start to loop in the image context. */
	printf("IL_FINISHED_LOOPING\n");
	break;

    default:
	break;
    }
#endif
    QtContext::qt(context)->updateStopState();
}








/* Create and initialize the Image Library JMC callback interface.
   Also create an IL_GroupContext for the current context. */

bool QtBrowserContext::initImageCallbacks()
{
    IL_GroupContext *img_cx;
    IMGCB* img_cb;
    JMCException *exc = 0;

    mwContext()->convertPixX = mwContext()->convertPixY = 1;
    IL_RGBBits rgb;           /* RGB bit allocation and shifts. */

    rgb.green_bits = 8;
    rgb.blue_bits = 8;
    rgb.red_bits = 8;
    rgb.green_shift = 8;
    rgb.red_shift = 0;
    rgb.blue_shift = 16;
    mwContext()->color_space = IL_CreateTrueColorSpace(&rgb, 32);






    if (!mwContext()->img_cx) {
        PRBool observer_added_p;

        img_cb = IMGCBFactory_Create(&exc); /* JMC Module */
        if (exc) {
            JMC_DELETE_EXCEPTION(&exc); /* XXXM12N Should really return
                                           exception */
            return false;
        }

        /* Create an Image Group Context.  IL_NewGroupContext augments the
           reference count for the JMC callback interface.  The opaque argument
           to IL_NewGroupContext is the Front End's display context, which will
           be passed back to all the Image Library's FE callbacks. */
        img_cx = IL_NewGroupContext((void*)mwContext(), (IMGCBIF *)img_cb);

        /* Attach the IL_GroupContext to the FE's display context. */
        mwContext()->img_cx = img_cx;

	// probably temporary (Matthias)
	IL_DisplayData dpy_data;
	dpy_data.color_space = mwContext()->color_space;
	dpy_data.progressive_display = (PRBool)TRUE;
	dpy_data.dither_mode =  IL_Auto;
	uint32 display_prefs = IL_COLOR_SPACE | IL_PROGRESSIVE_DISPLAY | IL_DITHER_MODE;
	IL_SetDisplayMode (img_cx, display_prefs, &dpy_data);



        /* Add an image group observer to the IL_GroupContext. */
	observer_added_p = IL_AddGroupObserver(img_cx, imageGroupObserver,
					       (void *)mwContext());
    }
    return true;
}


void QtBrowserContext::setDocTitle(char *title)
{
   MWContext *top_context = XP_GetNonGridContext( mwContext() );
   char *url;
    QString s;
   History_entry *he;
   he = SHIST_GetCurrent (&top_context->hist);
   url = (he && he->address ? he->address : 0);
   SHIST_SetTitleOfCurrentDoc (&top_context->hist, title);
    s.sprintf("%s (Mozilla)", title);
    browserWidget->setCaption(s);
}

void QtBrowserContext::refreshURLNow()
{
    killRefreshURLTimer();
    URL_Struct *url = NET_CreateURLStruct(refreshURL, NET_NORMAL_RELOAD);
    // debug( "QtBrowserContext::refreshURLNow: Timer expired, now getURL");
    getURL( url );
}


/* Header file comment: */
/* This is how the libraries ask the front end to load a URL just
   as if the user had typed/clicked it (thermo and busy-cursor
   and everything.)  NET_GetURL doesn't do all this stuff.
 */
/* From ./xfe.c: */
int QtBrowserContext::getURL(URL_Struct *url)
{
#if 0//defined DEBUG_paul
    debug( "QtBrowserContext::getUrl %s", url?url->address : "<null>" );
#endif

    MWContext *context = mwContext();

    int32 x,y;
    if (url && XP_FindNamedAnchor (context, url, &x, &y)) {
	setupToolbarSignals();
	emit urlChanged( url->address );
	mwContext()->url = qstrdup(url->address);
	setDocPosition( 0, x, y ); //### what is an iLocation ???
	return 0;
    } else {
	XP_InterruptContext(context);
	setupToolbarSignals();
	emit urlChanged( url->address );
	mwContext()->url = qstrdup(url->address);
	return NET_GetURL( url, FO_CACHE_AND_PRESENT, mwContext(), url_exit);
    }
}


void QtBrowserContext::browserGetURL( const char* address )
{
#if 0//defined DEBUG_paul
    debug( "browserGetURL %s", address );
#endif

   getURL( NET_CreateURLStruct( address, NET_DONT_RELOAD ));
}


void QtBrowserContext::editorEdit( Chrome*, const char* address )
{
    unimplemented( "QtBrowserContext::editEditor %s", address );
}


void QtBrowserContext::setRefreshURLTimer(uint32 secs, char *url)
{
    killRefreshURLTimer();
    refreshURL = url;
    if ( secs <= 0 ) {
	refreshURLNow();
    } else {
	refreshURLTimer = new QTimer( this );
	connect( refreshURLTimer, SIGNAL(timeout()), SLOT(refreshURLNow()) );
	refreshURLTimer->start( secs*1000, TRUE /* single shot */ );
    }
}


void QtBrowserContext::killRefreshURLTimer()
{
    delete refreshURLTimer;
    refreshURLTimer = 0;
}


void QtBrowserContext::setBackgroundColor(uint8 red, uint8 green, uint8 blue)
{
    contentWidget()->setBackgroundColor( QColor(red,green,blue) );
    // We should set the tranparent_pixel somewhere
}


bool QtBrowserContext::securityDialog( int state, char *prefs_toggle )
{
    SecurityDialog* dlg = new SecurityDialog( state, prefs_toggle,
					      contentWidget(),
					      "securitydialog" );
    int ret = dlg->exec();
    *prefs_toggle = dlg->prefsToggleState();
    delete dlg;

    return ret == QDialog::Accepted ? true : false;
}

/* From ./src/context_funcs.cpp: */
int QtBrowserContext::enableBackButton()
{
    if (toolbars)
	toolbars->setBackButtonEnabled( true );
    if (browserWidget->menuBar())
	browserWidget->menuBar()->setItemEnabled( MENU_BF_GO_BACK, true );
    if ( contextMenu )
	contextMenu->setItemEnabled( ContextMenu::Back, true );
    return 0;
}

/* From ./src/context_funcs.cpp: */
int QtBrowserContext::enableForwardButton()
{
  if (toolbars)
      toolbars->setForwardButtonEnabled( true );
  if (browserWidget->menuBar())
      browserWidget->menuBar()->setItemEnabled( MENU_BF_GO_FORWARD, true );
  if ( contextMenu )
      contextMenu->setItemEnabled( ContextMenu::Forward, true );
  return 0;
}

/* From ./src/context_funcs.cpp: */
int QtBrowserContext::disableBackButton()
{
    if (toolbars)
	toolbars->setBackButtonEnabled( false );
    if (browserWidget->menuBar())
	browserWidget->menuBar()->setItemEnabled( MENU_BF_GO_BACK, false );
    if ( contextMenu )
	contextMenu->setItemEnabled( ContextMenu::Back, false );
    return 0;
}

/* From ./src/context_funcs.cpp: */
int QtBrowserContext::disableForwardButton()
{
    if (toolbars)
	toolbars->setForwardButtonEnabled( false );
    if (browserWidget->menuBar())
	browserWidget->menuBar()->setItemEnabled( MENU_BF_GO_FORWARD, false );
    if ( contextMenu )
	contextMenu->setItemEnabled( ContextMenu::Forward, true );
    return 0;
}

void QtBrowserContext::cmdOpenBrowser()
{
    char*       address = NULL;
    URL_Struct* url = NULL;

    if( address != NULL ) {
	url = NET_CreateURLStruct( address, NET_DONT_RELOAD );
    } else {
	QString clb( QApplication::clipboard()->text() );
	if ( clb.find( "http://", 0, FALSE ) == 0 ||
	     clb.find( "www.", 0, TRUE ) == 0 ) {
	    clb = clb.simplifyWhiteSpace();
	    url = NET_CreateURLStruct( clb, NET_DONT_RELOAD );
	}
    }

    unimplemented( "Where do we get these parameters from? Kalle" );
    makeNewWindow( url, 0, 0, 0 );
}

void QtBrowserContext::cmdComposeMessage()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdComposeMessage" );
}

void QtBrowserContext::cmdNewBlank()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdNewBlank" );
}

void QtBrowserContext::cmdNewTemplate()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdNewTemplate" );
}

void QtBrowserContext::cmdNewWizard()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdNewWizard" );
}

void QtBrowserContext::cmdOpenPage()
{
    if( !dialogpool->opendialog )
	dialogpool->opendialog = new OpenDialog( this, browserWidget );
    dialogpool->opendialog->exec();
}

void QtBrowserContext::cmdSaveFrameAs()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdSaveFrameAs" );
}

void QtBrowserContext::cmdSendPage()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdSendPage" );
}

void QtBrowserContext::cmdSendLink()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdSendLink" );
}

void QtBrowserContext::cmdEditFrame()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdEditFrame" );
}

void QtBrowserContext::cmdEditPage()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdEditPage" );
}

void QtBrowserContext::cmdUploadFile()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdUploadFile" );
}

void QtBrowserContext::cmdClose()
{
    //window->close();
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdClose" );
}

void QtBrowserContext::cmdUndo()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdUndo" );
}

void QtBrowserContext::cmdRedo()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdRedo" );
}

void QtBrowserContext::cmdCut()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdCut" );
}

void QtBrowserContext::cmdCopy()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdCopy" );
}

void QtBrowserContext::cmdPaste()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdPaste" );
}

void QtBrowserContext::cmdSelectAll()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdSelectAll" );
}

void QtBrowserContext::cmdFindInObject()
{
    if( !dialogpool->finddialog )
	dialogpool->finddialog = new FindDialog( this, 0 );

    if( !dialogpool->finddialog->isVisible() )
	dialogpool->finddialog->show();
    else
	dialogpool->finddialog->raise();
}

void QtBrowserContext::cmdFindAgain()
{
    static firstFind = true;
    if( firstFind ) {
	firstFind = false;
	cmdFindInObject();
    } else {
	if( find_data.find_in_headers ) {
	    XP_ASSERT(find_data.context->type == MWContextMail ||
		      find_data.context->type == MWContextNews );
	    if( find_data.context->type == MWContextMail ||
		find_data.context->type == MWContextNews ) {
		return;
	    }
	}
	dialogpool->finddialog->find();
    }
}

void QtBrowserContext::cmdSearch()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdSearch" );
}

void QtBrowserContext::cmdSearchAddress()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdSearchAddress" );
}

void QtBrowserContext::cmdEditPreferences()
{
    QtPrefs::editPreferences( this );
}

void QtBrowserContext::cmdToggleNavigationToolbar()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdToggleNavigationToolbar" );
}

void QtBrowserContext::cmdToggleLocationToolbar()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdToggleLocationToolbar" );
}

void QtBrowserContext::cmdTogglePersonalToolbar()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdTogglePersonalToolbar" );
}

void QtBrowserContext::cmdIncreaseFont()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdIncreaseFont" );
}

void QtBrowserContext::cmdDecreaseFont()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdDecreaseFont" );
}

void QtBrowserContext::cmdReload()
{

  if (mwContext()->url){
    URL_Struct* url = NET_CreateURLStruct (mwContext()->url, NET_NORMAL_RELOAD);
    getURL(url);
  }
}

void QtBrowserContext::cmdSuperReload()
{

  if (mwContext()->url){
    URL_Struct* url = NET_CreateURLStruct (mwContext()->url, NET_SUPER_RELOAD);
    getURL(url);
  }
}

void QtBrowserContext::cmdShowImages()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdShowImages" );
}

void QtBrowserContext::cmdRefresh()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdRefresh" );
}

void QtBrowserContext::cmdStopLoading()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdStopLoading" );
}

void QtBrowserContext::cmdViewPageSource()
{
    History_entry *h;
    URL_Struct *url = 0;
    char *new_url_add=0;

    h = SHIST_GetCurrent (&mwContext()->hist);
    url = SHIST_CreateURLStructFromHistoryEntry(mwContext(), h);
    if (! url) {
	FE_Alert (mwContext(),
		  tr( "No document has yet been loaded in this window." ) );
	return;
    }

    /* check to make sure it doesn't already have a view-source
     * prefix.  If it does then this window is already viewing
     * the source of another window.  In this case just
     * show the same thing by reloading the url...
     */
    if(strncmp( VIEW_SOURCE_URL_PREFIX,
		url->address,
		sizeof(VIEW_SOURCE_URL_PREFIX)-1)) {
	/* prepend VIEW_SOURCE_URL_PREFIX to the url to
	 * get the netlib to display the source view
	 */
	StrAllocCopy(new_url_add, VIEW_SOURCE_URL_PREFIX);
	StrAllocCat(new_url_add, url->address);
	free(url->address);
	url->address = new_url_add;
    }

    /* make damn sure the form_data slot is zero'd or else all
     * hell will break loose
     */
    XP_MEMSET (&url->savedData, 0, sizeof (SHIST_SavedData));

#ifdef EDITOR
    //    if (EDT_IS_EDITOR(mwContext()) && !FE_CheckAndSaveDocument(mwContext()))
    //	return;
#endif

    getURL( url );
}

#define DOCINFO			"about:document"
#define HTTP_OFF		42

void QtBrowserContext::cmdViewPageInfo()
{
    char buf [1024], *in, *out;
    for (in = DOCINFO, out = buf; *in; in++, out++) *out = *in - HTTP_OFF;
    *out = 0;

#ifdef EDITOR
    //    if (EDT_IS_EDITOR(mwContext()) && !FE_CheckAndSaveDocument(mwContext()))
    //	return;
#endif
    /* @@@@ this is the proper way to do it but I dont'
     * know the encoding scheme
     * fe_GetURL (context,NET_CreateURLStruct(buf, FALSE), FALSE);
     */
    getURL ( NET_CreateURLStruct( DOCINFO, NET_DONT_RELOAD ) );

#ifdef EDITOR
    //    fe_EditorRefresh(context);
#endif
}

void QtBrowserContext::cmdPageServices()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdPageServices" );
}

void QtBrowserContext::cmdBack()
{
    MWContext *top_context = XP_GetNonGridContext( mwContext() );
    URL_Struct *url;
    if( isGridParent( top_context ) ) {
	if( LO_BackInGrid( top_context ) ) {
	    return;
	}
    }
    //#warning Implement reordering of contexts. Kalle.
    //    fe_UserActivity( top_context );
    url = SHIST_CreateURLStructFromHistoryEntry( top_context,
						 SHIST_GetPrevious( top_context ) );
    if (url) {
      qt(top_context)->getURL(url);
    } else {
	FE_Alert( top_context, tr( "No previous document." ) );
    }
}

void QtBrowserContext::cmdForward()
{
    MWContext *top_context = XP_GetNonGridContext( mwContext() );
    URL_Struct *url;
    if( isGridParent( top_context ) ) {
	if( LO_ForwardInGrid( top_context ) ) {
	    return;
	}
    }
    //#warning Implement reordering of contexts. Kalle.
    //    fe_UserActivity( top_context );
    url = SHIST_CreateURLStructFromHistoryEntry( top_context,
						 SHIST_GetNext( top_context ) );
    if (url) {
      qt(top_context)->getURL(url);
    } else {
	FE_Alert( top_context, tr( "No previous document." ) );
    }
}

void QtBrowserContext::cmdHome()
{
  URL_Struct *url;

#define BUF_SIZ 1024
  int bufsiz = BUF_SIZ;
  char buf[ BUF_SIZ ];
  if( PREF_GetCharPref( "browser.startup.homepage", buf, &bufsiz ) < 0 || !strlen(buf) )
      {
	  clearView( 0 );
	  alert( tr( "No home document specified." ) );
	  return;
    }
  url = NET_CreateURLStruct ( buf, NET_DONT_RELOAD ); // ########hanord
  getURL ( url );
}

void QtBrowserContext::cmdOpenNavCenter()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdOpenNavCenter" );
}

void QtBrowserContext::cmdOpenOrBringUpBrowser()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdOpenOrBringUpBrowser" );
}

void QtBrowserContext::cmdOpenInbox()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdOpenInbox" );
}

void QtBrowserContext::cmdOpenNewsgroups()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdOpenNewsgroups" );
}

void QtBrowserContext::cmdOpenEditor()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdOpenEditor" );
}

void QtBrowserContext::cmdOpenConference()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdOpenConference" );
}

void QtBrowserContext::cmdCalendar()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdCalendar" );
}

void QtBrowserContext::cmdHostOnDemand()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdHostOnDemand" );
}

void QtBrowserContext::cmdNetcaster()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdNetcaster" );
}

void QtBrowserContext::cmdToggleTaskbarShowing()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdToggleTaskbarShowing" );
}

void QtBrowserContext::cmdOpenFolders()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdOpenFolders" );
}

void QtBrowserContext::cmdOpenAddressBook()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdOpenAdressBook" );
}

void QtBrowserContext::cmdOpenHistory()
{
//    QtHistoryContext *hist = QtHistoryContext::ptr();
//    hist->show();
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdOpenHistory" );
}

void QtBrowserContext::cmdJavaConsole()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdOpenHistory" );
}

void QtBrowserContext::cmdViewSecurity()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmnViewSecurity" );
}

void QtBrowserContext::cmdAboutNetscape()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdAboutNetscape" );
}

void QtBrowserContext::cmdAboutMozilla()
{
    unimplemented( "Sorry not implemented: QtBrowserContext::cmdAboutMozilla" );
}

void QtBrowserContext::cmdAboutQt()
{
    QMessageBox::aboutQt( 0 );
}


void QtBrowserContext::cmdGuide()
{
    unimplemented( "Sorry, not implemented: QtBrowserContext::cmdGuide" );
}


#include "SecurityDialog.h"
#include <fe_proto.h>
void QtBrowserContext::cmdSecurityDialog1()
{
    char prefs = true;
    SecurityDialog dlg( SD_INSECURE_POST_FROM_SECURE_DOC, &prefs, 0 );
    dlg.exec();
}

void QtBrowserContext::cmdSecurityDialog2()
{
    char prefs = true;
    SecurityDialog dlg( SD_INSECURE_POST_FROM_INSECURE_DOC, &prefs, 0 );
    dlg.exec();
}

void QtBrowserContext::cmdSecurityDialog3()
{
    char prefs = true;
    SecurityDialog dlg( SD_ENTERING_SECURE_SPACE, &prefs, 0 );
    dlg.exec();
}

void QtBrowserContext::cmdSecurityDialog4()
{
    char prefs = true;
    SecurityDialog dlg( SD_LEAVING_SECURE_SPACE, &prefs, 0 );
    dlg.exec();
}

void QtBrowserContext::cmdSecurityDialog5()
{
    char prefs = true;
    SecurityDialog dlg( SD_INSECURE_POST_FROM_INSECURE_DOC, &prefs, 0 );
    dlg.exec();
}

void QtBrowserContext::cmdSecurityDialog6()
{
    char prefs = true;
    SecurityDialog dlg( SD_REDIRECTION_TO_INSECURE_DOC, &prefs, 0 );
    dlg.exec();
}

void QtBrowserContext::cmdSecurityDialog7()
{
    char prefs = true;
    SecurityDialog dlg( SD_REDIRECTION_TO_SECURE_SITE, &prefs, 0 );
    dlg.exec();
}


void QtBrowserContext::setMessage( const char *m, int timeout )
{
    messageFlickerTimer->stop();
    emit messengerMessage( m, timeout );
    currentMessage = m;
}

void QtBrowserContext::setStopButtonEnabled( bool enable )
{
  if (toolbars)
    toolbars->setStopButtonEnabled( enable );
}


void QtBrowserContext::setLoadImagesButtonEnabled( bool enable )
{
  if (toolbars)
    toolbars->setLoadImagesButtonEnabled( enable );
}


void QtBrowserContext::contextMenuDied()
{
    contextMenu = 0;
}

void QtBrowserContext::showSubWidget( QWidget* w, bool y )
{
    scrollView->showChild( w, y );
}

void QtBrowserContext::moveSubWidget( QWidget* w, int x, int y )
{
    scrollView->addChild( w, x, y );
    scrollView->showChild( w, TRUE );
}

void QtBrowserContext::setupToolbarSignals()
{
    if ( !have_tried_progress && toolbars) {
	    QObjectListIt it( *browserWidget->children() );
	    QObject * o;
	    while( (o=it.current()) != 0 ) {
		++it;
		if ( o->inherits( "Toolbars" ) ) {
		    connect( this, SIGNAL(progressStarting(int)),
			     o, SLOT(setupProgress(int)) );
		    connect( this, SIGNAL(progressMade(int)),
			     o, SLOT(signalProgress(int)) );
		    connect( this, SIGNAL(progressMade()),
			     o, SLOT(signalProgress()) );
		    connect( this, SIGNAL(progressFinished()),
			     o, SLOT(endProgress()) );
		    connect( this, SIGNAL(progressReport(const char *)),
			     o, SLOT(setMessage(const char *)) );
		    connect( this, SIGNAL(canStop(bool)),
			     o, SLOT(setStopButtonEnabled(bool)) );
		    connect( this, SIGNAL(urlChanged(const char *)),
			     o, SLOT(setComboText(const char *)) );
		}
	    }
    }
    have_tried_progress = TRUE;
}


void QtBrowserContext::scrollUp()
{
    scrollView->scrollBy( 0, -scrollView->fontMetrics().height() );
}


void QtBrowserContext::scrollDown()
{
    scrollView->scrollBy( 0, scrollView->fontMetrics().height() );
}


void QtBrowserContext::pageUp()
{
	scrollView->scrollBy( 0, -scrollView->viewport()->height() );
}


void QtBrowserContext::pageDown()
{
	scrollView->scrollBy( 0, scrollView->viewport()->height() );
}
