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

#define HOWMANY(x, r)     (((x) + ((r) - 1)) / (r))
#define ROUNDUP(x, r)     (HOWMANY(x, r) * (r))

JMC_PUBLIC_API(void)
_IMGCB_init(struct IMGCB* self, JMCException* *exception)
{
  printf ("_IMGCB_init\n");
}

JMC_PUBLIC_API(void*)
_IMGCB_getBackwardCompatibleInterface(struct IMGCB* self,
				      const JMCInterfaceID* iid,
				      JMCException* *exception)
{
  printf ("_IMGCB_getBackwardCompatibleInterface\n");
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
  
  printf ("_IMGCB_NewPixmap\n");

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
  /*XXX toshok - XP_ASSERT(img_depth == pixmap_depth);*/
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
  
#if 0 /* XXX - toshok, for now */
  /* Create an X pixmap for the image, and for the mask (if required.) */
  img_x_pixmap = XCreatePixmap(dpy, window, img_header->width,
                               img_header->height, visual_depth);
  if (mask)
    mask_x_pixmap = XCreatePixmap(dpy, window, mask_header->width,
                                  mask_header->height, 1);
  
  /* Fill in the pixmap client_data.  We store the Display pointer for use
     in DestroyPixmap, which can be called after the FE's display context
     (MWContext) has been destroyed. */
  img_client_data->pixmap = img_x_pixmap;
  img_client_data->dpy = dpy;
  image->client_data = (void *)img_client_data;
  if (mask) {
    mask_client_data->pixmap = mask_x_pixmap;
    mask_client_data->dpy = dpy;
    mask->client_data = (void *)mask_client_data;
  }
#endif
}

JMC_PUBLIC_API(void)
_IMGCB_UpdatePixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* pixmap,
		    jint x_offset, jint y_offset, jint width, jint height)
{
  printf ("_IMGCB_UpdatePixmap\n");
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
  printf ("_IMGCB_DestroyPixmap\n");
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
