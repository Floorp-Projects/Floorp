/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* 
   gnomeimg.c --- gnome functions for fe
                  specific images stuff.
*/

#define JMC_INIT_IMGCB_ID

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"

#include "libimg.h"
#include "il_util.h"
#include "prtypes.h"
#include "g-util.h"
#include "g-types.h"
#include "gnomefe.h"
#include "g-html-view.h"
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/X.h>

#define HOWMANY(x, r)     (((x) + ((r) - 1)) / (r))
#define ROUNDUP(x, r)     (HOWMANY(x, r) * (r))

JMC_PUBLIC_API(void)
_IMGCB_init(struct IMGCB* self, JMCException* *exception)
{
  printf ("_IMGCB_init (nothing to do)\n");
}

JMC_PUBLIC_API(void*)
_IMGCB_getBackwardCompatibleInterface(struct IMGCB* self,
				      const JMCInterfaceID* iid,
				      JMCException* *exception)
{
  printf ("_IMGCB_getBackwardCompatibleInterface (nothing to do)\n");
}

JMC_PUBLIC_API(void)
_IMGCB_NewPixmap(IMGCB* img_cb, jint op, void *dpy_cx, jint width, jint height,
		 IL_Pixmap *image, IL_Pixmap *mask) 
{
  MWContext *context = (MWContext *)dpy_cx; /* XXX This should be the FE's
                                               display context. */
  uint8 img_depth;
  NI_PixmapHeader *img_header = &image->header;
  NI_PixmapHeader *mask_header = mask ? &mask->header : NULL;
  Pixmap img_x_pixmap=0, mask_x_pixmap = 0;
  fe_PixmapClientData *img_client_data, *mask_client_data = NULL;
  MozHTMLView *view = 0;
  unsigned int visual_depth;
  fe_Drawable *fe_drawable;

  printf ("_IMGCB_NewPixmap\n");

  /* Allocate the client data structures for the IL_Pixmaps. */
  img_client_data = XP_NEW_ZAP(fe_PixmapClientData);
  if (!img_client_data) {
    image->bits = NULL;
    mask->bits = NULL;
    return;
  }
  if (mask) {
    mask_client_data = XP_NEW_ZAP(fe_PixmapClientData);
    if (!mask_client_data) {
            image->bits = NULL;
            mask->bits = NULL;
            return;
    }
  }
  
  /* try to get the required display parameters from the html view */
  view = find_html_view(context);
  XP_ASSERT(view);
  fe_drawable = view->drawable;
  visual_depth = gdk_visual_get_best_depth();
  
  /* Override the image and mask dimensions with the requested target
     dimensions.  This instructs the image library to do any necessary
     scaling. */
  img_header->width = width;
  img_header->height = height;
  if (mask) {
    mask_header->width = width;
    mask_header->height = height;
  }
  
  /* Override the image colorspace with the display colorspace.  This
     instructs the image library to decode to the display colorspace
     instead of decoding to the image's source colorspace. */
  IL_ReleaseColorSpace(img_header->color_space);
  img_header->color_space = context->color_space;
  IL_AddRefToColorSpace(img_header->color_space);
  
  /* Compute the number of bytes per scan line for the image and mask,
     and make sure it is quadlet aligned. */

  img_depth = img_header->color_space->pixmap_depth;
  XP_ASSERT(img_depth == visual_depth);
  img_header->widthBytes = (img_header->width * img_depth + 7) / 8;
  img_header->widthBytes = ROUNDUP(img_header->widthBytes, 4);
  if (mask) {
    mask_header->widthBytes = (mask_header->width + 7) / 8;
    mask_header->widthBytes = ROUNDUP(mask_header->widthBytes, 4);
  }

  /* Allocate memory for the image bits, and for the mask bits (if
     required.) */
  image->bits = calloc(img_header->widthBytes * img_header->height, 1);
  if (!image->bits)
    return;
  if (mask) {
    mask->bits = calloc(mask_header->widthBytes * mask_header->height, 1);
    if (!mask->bits) {
      free(image->bits);
      image->bits = NULL;
      return;
    }
  }
  
  /* Create an X pixmap for the image, and for the mask (if required.) */
  img_client_data->gdk_pixmap =
    gdk_pixmap_new((GdkWindow *)fe_drawable->drawable,
                   img_header->width,
                   img_header->height,
                   visual_depth);
  if (mask)
    mask_client_data->gdk_pixmap =
      gdk_pixmap_new((GdkWindow *)fe_drawable->drawable,
                     mask_header->width,
                     mask_header->height,
                     1);

  /* Fill in the pixmap client_data. */
  image->client_data = (void *)img_client_data;
  if (mask)
    mask->client_data = (void *)mask_client_data;
}

JMC_PUBLIC_API(void)
_IMGCB_UpdatePixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* pixmap,
		    jint x_offset, jint y_offset, jint width, jint height)
{
  uint32 widthBytes;
  MWContext *context = (MWContext *)dpy_cx; /* XXX This should be the FE's
                                                 display context. */
  unsigned int visual_depth;
  unsigned int pixmap_depth;  /* Depth of the IL_Pixmap. */
  Visual *visual;
  Pixmap x_pixmap;
  Display *dpy;
  XImage *x_image;
  GC gc;
  XGCValues gcv;
  IL_ColorSpace *color_space = pixmap->header.color_space;
  char *bits;
  fe_PixmapClientData *pixmap_client_data;
  fe_Drawable *fe_drawable;
  MozHTMLView *view = 0;
  GdkVisual   *gdk_visual;
  GdkPixmap   *gdk_pixmap;
  GdkImage    *gdk_image;
  GdkGC       *gdk_gc;
  GdkGCValues *gdkgc_values;


  printf ("_IMGCB_UpdatePixmap\n");

  if (!context)
    return;

  /* get the client data */
  pixmap_client_data = (fe_PixmapClientData *)pixmap->client_data;
  view = find_html_view(context);
  XP_ASSERT(view);
  fe_drawable = view->drawable;
  gdk_visual = gdk_window_get_visual((GdkWindow *)fe_drawable->drawable);
  if (gdk_visual == NULL) {
    printf("Warning: gdk_visual in _IMGCB_UpdatePixmap is null.\n");
    return;
  }
  visual = GDK_VISUAL_XVISUAL(gdk_visual);
  /* try to get the required display parameters from the pixmap */
  visual_depth = gdk_visual_get_best_depth();
  dpy = GDK_WINDOW_XDISPLAY(pixmap_client_data->gdk_pixmap);
  
  /* Check for zero dimensions. */
  if (width == 0 || height == 0)
    return;
  
  /* Get the depth of the IL_Pixmap.  This should be the same as the
     visual_pixmap_depth if the IL_Pixmap is an image, or 1 if the
     IL_Pixmap is a mask. */
  pixmap_depth = color_space->pixmap_depth;

  widthBytes = pixmap->header.widthBytes;
  bits = (unsigned char *)pixmap->bits + widthBytes * y_offset;

  x_image = XCreateImage(dpy, visual,
                         (pixmap_depth == 1 ? 1 : visual_depth),
                         (pixmap_depth == 1 ? XYPixmap : ZPixmap),
                         x_offset,                   /* offset */
                         bits,
                         width,
                         height,
                         8,                          /* bitmap_pad */
                         widthBytes);                /* bytes_per_line */

  if (pixmap_depth == 1) {
    /* Image library always places pixels left-to-right MSB to LSB */
    x_image->bitmap_bit_order = MSBFirst;
    
    /* This definition doesn't depend on client byte ordering
       because the image library ensures that the bytes in
       bitmask data are arranged left to right on the screen,
       low to high address in memory. */
    x_image->byte_order = MSBFirst;
  }
  else {
#if defined(IS_LITTLE_ENDIAN)
    x_image->byte_order = LSBFirst;
#elif defined (IS_BIG_ENDIAN)
    x_image->byte_order = MSBFirst;
#else
    ERROR! Endianness is unknown;
#endif
  }
  
  pixmap_client_data = (fe_PixmapClientData *)pixmap->client_data;
  x_pixmap = GDK_WINDOW_XWINDOW(pixmap_client_data->gdk_pixmap);
  
  memset(&gcv, ~0, sizeof(XGCValues));
  gcv.function = GXcopy;
  
  /* The pixmap_depth does not correspond to that of the GCs in the
     cache, so create a new GC. */
  gc = XCreateGC(dpy, x_pixmap, GCFunction, &gcv);
  
  XPutImage(dpy, x_pixmap, gc, x_image, x_offset, 0, x_offset,
            y_offset, width, height);
  
  XFreeGC(dpy, gc);
  x_image->data = 0;          /* Don't free the IL_Pixmap's bits. */
  XDestroyImage(x_image);
}

JMC_PUBLIC_API(void)
_IMGCB_ControlPixmapBits(IMGCB* img_cb, jint op, void* dpy_cx,
                         IL_Pixmap* pixmap, IL_PixmapControl message)
{
  printf ("_IMGCB_ControlPixmapBits\n");
}

JMC_PUBLIC_API(void)
_IMGCB_DestroyPixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* pixmap)
{
  fe_PixmapClientData *pixmap_client_data;

  printf ("_IMGCB_DestroyPixmap\n");
  pixmap_client_data = (fe_PixmapClientData *)pixmap->client_data;
  if (!pixmap_client_data)
    return;
  if (pixmap_client_data->gdk_pixmap)
    gdk_pixmap_unref(pixmap_client_data->gdk_pixmap);
  if (pixmap->bits) {
    free(pixmap->bits);
    pixmap->bits = NULL;
  }
}

JMC_PUBLIC_API(void)
_IMGCB_DisplayPixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* image, 
                     IL_Pixmap* mask, jint x, jint y, jint x_offset,
                     jint y_offset, jint width, jint height, jint j, jint k)
{
  printf ("_IMGCB_DisplayPixmap\n");
}

JMC_PUBLIC_API(void)
_IMGCB_GetIconDimensions(IMGCB* img_cb, jint op, void* dpy_cx, int* width,
                         int* height, jint icon_number)
{
  printf ("_IMGCB_GetIconDimensions\n");
}

JMC_PUBLIC_API(void)
_IMGCB_DisplayIcon(IMGCB* img_cb, jint op, void* dpy_cx, jint x, jint y,
                   jint icon_number)
{
  printf ("_IMGCB_DisplayIcon\n");
}

/* Mocha image group observer callback. */
void
FE_MochaImageGroupObserver(XP_Observable observable, XP_ObservableMsg message,
                           void *message_data, void *closure)
{
}



