/* $Id: image.cpp,v 1.1 1998/09/25 18:01:34 ramiro%netscape.com Exp $
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

#define JMC_INIT_IMGCB_ID

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"

#include "libimg.h"
#include "il_util.h"
#include "prtypes.h"

#include "QtContext.h"
#include "icons.h"
#include <qimage.h>
#include <qpainter.h>


class QtImageData {
public:
    QtImageData( MWContext *context, IL_Pixmap *ilpm, int w, int h, int d )
    {
	header = &ilpm->header;
	header->width = w;
	header->height = h;
        IL_ReleaseColorSpace(header->color_space);
        header->color_space = context->color_space;
        IL_AddRefToColorSpace(header->color_space);

	if ( d == 1 ) {
	    image = QImage( w, h, 1, 2, QImage::BigEndian );
	    image.setColor( 0, qRgb(0xff,0xff,0xff) );
	    image.setColor( 1, qRgb(0,0,0) );
	    header->widthBytes = ((w+31)/32)*4;
	} else {
	    image = QImage( w, h, 32 );
	    int img_depth = header->color_space->pixmap_depth;
	    header->widthBytes = ((w * img_depth + 31)/32)*4;
	}
	ilpm->bits = image.bits();
	ilpm->client_data = this;
    }

    void update(int x, int y, int w, int h)
    {
	if ( pixmap.isNull() ) {
	    pixmap = QPixmap( header->width, header->height,
			      image.depth() > 1 ? -1 : 1 );
//debug("IMG: ??:  create  %dx%d",header->width, header->height);
	}
	bitBlt( &pixmap, x, y, &image, x, y, w, h );
    }

    void message( IL_PixmapControl m )
    {
        if ( m == IL_RELEASE_BITS )
	    image = QImage();
    }

    NI_PixmapHeader* header;
    QPixmap    pixmap;
    QImage     image;
};


static void useArgs( const char *fn, ... )
{
    if (0&&fn) printf( "%s\n", fn );
}


extern "C"
JMC_PUBLIC_API(void)
_IMGCB_init(struct IMGCB* self, JMCException* *exception)
{
    useArgs("_IMGCB_init", self, exception);
}

extern "C"
JMC_PUBLIC_API(void*)
_IMGCB_getBackwardCompatibleInterface(struct IMGCB* self,
				      const JMCInterfaceID* iid,
				      JMCException* *exception)
{
    useArgs("_IMGCB_getBackwardCompatibleInterface", self, iid, exception);
    return 0;
}

extern "C"
JMC_PUBLIC_API(void)
_IMGCB_NewPixmap(IMGCB* img_cb, jint op, void *dpy_cx, jint width, jint height,
		 IL_Pixmap *image, IL_Pixmap *mask) 
{
  useArgs( 0, img_cb, op );
  ASSERT( width > 0 && height > 0 );
  new QtImageData( (MWContext*)dpy_cx, image, width, height, 32 );
  if ( mask )
    new QtImageData( (MWContext*)dpy_cx, mask, width, height, 1 );
}

extern "C"
JMC_PUBLIC_API(void)
_IMGCB_UpdatePixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* pixmap,
		    jint x_offset, jint y_offset, jint width, jint height)
{
//debug("IMG: %p:  update  %d,%d %dx%d",pixmap,x_offset,y_offset,width,height);
    useArgs(0,img_cb, op, dpy_cx );
    ((QtImageData *)pixmap->client_data)->
	update(x_offset,y_offset,width,height);
}

extern "C"
JMC_PUBLIC_API(void)
_IMGCB_ControlPixmapBits(IMGCB* img_cb, jint op, void* dpy_cx,
                         IL_Pixmap* pixmap, IL_PixmapControl message)
{
    ((QtImageData *)pixmap->client_data)->message( message );
    useArgs( 0, img_cb, op, dpy_cx, pixmap, message );
}

extern "C"
JMC_PUBLIC_API(void)
_IMGCB_DestroyPixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* pixmap)
{
    useArgs( 0, img_cb, op, dpy_cx, pixmap );
    QtImageData *qt_pixmap = (QtImageData *)pixmap->client_data;
    delete qt_pixmap;
    pixmap->client_data = 0;
    pixmap->bits = 0;
}


static void internalDisplayPixmap( void* dpy_cx, QPixmap pix, 
                     QPixmap *mask, jint x, jint y, jint x_offset,
                     jint y_offset, jint width, jint height)
{
    MWContext *context = (MWContext *)dpy_cx;
    if ( !pix.isNull() ) {
        QtContext *c = QtContext::qt(context);
        if ( c && c->contentWidget() ) {
            if ( mask && !mask->isNull() )
                pix.setMask( *((QBitmap*)mask) );
	    QPixmap pm = pix;

	    /* BUG - while not using backingstore
               Sometimes width and height look like region-style sizes,
               which are one more than IMGCBIF_DisplayPixmap expects. 
               Attempt to detect and correct this case.  
	       Should be fixed in the backend.
	    */
	    if ( x_offset+width == pm.width() + 1 ) width--;
	    if ( y_offset+height == pm.height() + 1 ) height--;

	    int img_x_offset = x - c->documentXOffset() + c->getXOrigin();
	    int img_y_offset = y - c->documentYOffset() + c->getYOrigin();
	    int rect_x_offset = img_x_offset + x_offset;
	    int rect_y_offset = img_y_offset + y_offset;
	    if ( x_offset + width > pm.width()
	      || y_offset + height > pm.height() )
	    {
		c->painter()->drawTiledPixmap(
			rect_x_offset, rect_y_offset,
			width, height, pm, x_offset, y_offset
		    );
	    } else {
		c->painter()->drawPixmap(
			rect_x_offset, rect_y_offset, pm,
			x_offset, y_offset, width, height
		    );
	    }
        }
    }

}


extern "C"
JMC_PUBLIC_API(void)
_IMGCB_DisplayPixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* image, 
                     IL_Pixmap* mask, jint x, jint y, jint x_offset,
                     jint y_offset, jint width, jint height, jint /*req_w*/, jint /*req_h*/)
{
    if ( width <= 0 || height <= 0 )
      return;

    QtImageData *qt_image = (QtImageData *)image->client_data;
    QtImageData *qt_mask  = mask ? (QtImageData *)mask->client_data : 0;

    if ( qt_image ) 
      internalDisplayPixmap( dpy_cx, qt_image->pixmap,
                             qt_mask ? &(qt_mask->pixmap) : 0,
                             x, y, x_offset,
                             y_offset, width, height);
}

extern "C"
JMC_PUBLIC_API(void)
_IMGCB_GetIconDimensions(IMGCB* img_cb, jint op, void* dpy_cx, int* width,
                         int* height, jint icon_number)
{
    Pixmap pm( icon_number );
    *width = pm.width();
    *height = pm.height();

    //  if ( *width <= 0 || *height <= 0 )
    //    *width = *height = 50;
}

extern "C"
JMC_PUBLIC_API(void)
_IMGCB_DisplayIcon(IMGCB* img_cb, jint op, void* dpy_cx, jint x, jint y,
                   jint icon_number)
{
    Pixmap pm( icon_number );
    int width = pm.width();
    int height = pm.height();


    internalDisplayPixmap( dpy_cx, pm, 0,
                           x, y, 0, 0, //##### offset???
                           width, height );

    //    useArgs("_IMGCB_DisplayIcon", img_cb, op, dpy_cx, x, y, icon_number);
}

/* Mocha image group observer callback. */
extern "C"
void
FE_MochaImageGroupObserver(XP_Observable observable, XP_ObservableMsg message,
                           void *message_data, void *closure)
{   
    useArgs("FE_MochaImageGroupObserver", observable, message, message_data, closure);
}
