/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
** All the wonderful code for dealing with images
*/
#define JMC_INIT_PSIMGCB_ID

#include "xlate_i.h"
#include "libimg.h"             /* Image Library public API. */
#include "il_util.h"            /* Colormap/Colorspace API. */

/* Create and initialize the Image Library JMC callback interface.
   Also create an IL_GroupContext for the current context. */
XP_Bool
psfe_init_image_callbacks(MWContext *cx)
{
    IL_GroupContext *img_cx;
    PSIMGCB* img_cb;
    JMCException *exc = NULL;

    if (!cx->img_cx) {
        img_cb = PSIMGCBFactory_Create(&exc); /* JMC Module */
        if (exc) {
            JMC_DELETE_EXCEPTION(&exc); /* XXXM12N Should really return
                                           exception */
            return FALSE;
        }

        /* Create an Image Group Context.  IL_NewGroupContext augments the
           reference count for the JMC callback interface.  The opaque argument
           to IL_NewGroupContext is the Front End's display context, which will
           be passed back to all the Image Library's FE callbacks. */
        img_cx = IL_NewGroupContext((void*)cx, (IMGCBIF *)img_cb);

        /* Attach the IL_GroupContext to the FE's display context. */
        cx->img_cx = img_cx;
    }
    return TRUE;
}
 
/*****************************************************************************/
/*                       Image Library callbacks                             */
/*****************************************************************************/

#define HOWMANY(x, r)     (((x) + ((r) - 1)) / (r))
#define ROUNDUP(x, r)     (HOWMANY(x, r) * (r))

JMC_PUBLIC_API(void)
_PSIMGCB_init(struct PSIMGCB* self, JMCException* *exception)
{
    /* Nothing to be done here. */
}

extern JMC_PUBLIC_API(void*)
_PSIMGCB_getBackwardCompatibleInterface(struct PSIMGCB* self,
                                        const JMCInterfaceID* iid,
                                        JMCException* *exception)
{
    return NULL;                /* Nothing to be done here. */
}


/**************************** Pixmap creation ********************************/
JMC_PUBLIC_API(void)
_PSIMGCB_NewPixmap(PSIMGCB* img_cb, jint op, void *dpy_cx, jint width,
                   jint height, IL_Pixmap *image, IL_Pixmap *mask)
{
    uint8 img_depth, pixmap_depth;
    NI_PixmapHeader *img_header = &image->header;
    NI_PixmapHeader *mask_header = mask ? &mask->header : NULL;
    MWContext *context = (MWContext *)dpy_cx; /* XXXM12N This should be the
                                                 FE's display context. */


    /* Determine the depth of the image. */
    pixmap_depth = context->color_space->pixmap_depth;

    /* Override the image colorspace with the display colorspace.  This
       instructs the image library to decode to the display colorspace
       instead of decoding to the image's source colorspace. */
    IL_ReleaseColorSpace(img_header->color_space);
    img_header->color_space = context->color_space;
    IL_AddRefToColorSpace(img_header->color_space);

    /* Ask the image library to scale to the requested dimensions.
       Context-specific scaling, however, will be handled by the PSFE. */
    img_header->width = width;
    img_header->height = height;
    if (mask) {
        mask_header->width = width;
        mask_header->height = height;
    }
    
    /* Compute the number of bytes per scan line for the image and mask,
       and make sure it is quadlet aligned. */
    img_depth = img_header->color_space->pixmap_depth;
    XP_ASSERT(img_depth == pixmap_depth);
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
}


/**************************** Pixmap update **********************************/
JMC_PUBLIC_API(void)
_PSIMGCB_UpdatePixmap(PSIMGCB* img_cb, jint op, void* dpy_cx,
                      IL_Pixmap* pixmap, jint x_offset, jint y_offset,
                      jint width, jint height)
{
}


/************************** Pixmap memory management *************************/
JMC_PUBLIC_API(void)
_PSIMGCB_ControlPixmapBits(PSIMGCB* img_cb, jint op, void* dpy_cx,
                           IL_Pixmap* pixmap, IL_PixmapControl message)
{
}


/**************************** Pixmap destruction *****************************/
/* XXXM12N The dpy_cx argument is not used in DestroyPixmap and should be
   removed. */
JMC_PUBLIC_API(void)
_PSIMGCB_DestroyPixmap(PSIMGCB* img_cb, jint op, void* dpy_cx,
                       IL_Pixmap* pixmap)
{
    /* Free the pixmap's bits. */
    if (pixmap->bits) {
        free(pixmap->bits);
        pixmap->bits = NULL;
    }
}


/**************************** Pixmap display *********************************/
JMC_PUBLIC_API(void)
_PSIMGCB_DisplayPixmap(PSIMGCB* img_cb, jint op, void* dpy_cx,
                       IL_Pixmap* image, IL_Pixmap* mask, jint x, jint y,
                       jint x_offset, jint y_offset, jint width, jint height)
{
    int32 img_x_offset, img_y_offset;   /* Offset of image in drawable. */
    int32 rect_x_offset, rect_y_offset; /* Offset of update rect in
                                           drawable. */
    NI_PixmapHeader *img_header = &image->header;
    uint32 img_width = img_header->width;     /* Image width. */
    uint32 img_height = img_header->height;   /* Image height. */
    MWContext *context = (MWContext *)dpy_cx; /* XXX This should be the FE's
                                                 display context. */
    XP_Bool tiling_required = FALSE;

    /* Check for zero display area. */
    if (width == 0 || height == 0)
        return;

    /* Perform context scaling. */
    x *= context->convertPixX;
    y *= context->convertPixY;
    x_offset *= context->convertPixX;
    y_offset *= context->convertPixY;
    width *= context->convertPixX;
    height *= context->convertPixY;
    img_width *= context->convertPixX;
    img_height *= context->convertPixY;

    /* Determine whether tiling is required. */
    if ((x_offset + width > img_width) || (y_offset + height > img_height))
        tiling_required = TRUE;

    /* Compute the offset into the drawable of the image origin. */
    img_x_offset = x; /* + fe_drawable->x_origin; */
    img_y_offset = y; /* + fe_drawable->y_origin; */

    /* Check whether the image is ready to be displayed. */
    if (!XP_CheckElementSpan(context, img_y_offset, img_height))
        return;

    /* Draw the image. */
    xl_colorimage(context, x, y, width, height, image, mask);
}


/**************************** Icon dimensions ********************************/
JMC_PUBLIC_API(void)
_PSIMGCB_GetIconDimensions(PSIMGCB* img_cb, jint op, void* dpy_cx, int* width,
                           int* height, jint icon_number)
{
    /* Call the screen FE to get the icon dimensions in pixels. */
    FE_GetPSIconDimensions(icon_number, width, height);
}


/**************************** Icon display ***********************************/
JMC_PUBLIC_API(void)
_PSIMGCB_DisplayIcon(PSIMGCB* img_cb, jint op, void* dpy_cx, jint x, jint y,
                     jint icon_number)
{
    int width;       /* Width of the icon in context scaled coordinates. */
    int height;      /* Height of the icon in context scaled coordinates. */
    int img_depth;   /* Depth of the image. */
    MWContext *context = (MWContext *)dpy_cx;
    IL_Pixmap image; /* Image pixmap. */
    NI_PixmapHeader *img_header = &image.header;

    /* Call the screen FE to get the icon dimensions in pixels. */
    FE_GetPSIconDimensions(icon_number, &width, &height);
    img_header->width = (int32)width;
    img_header->height = (int32)height;

    /* Apply context scaling to the dimensions. */
    width *= context->convertPixX;
    height *= context->convertPixY;

    /* Check whether we are ready to display the icon. */
    if (!XP_CheckElementSpan(context, y, height))
        return;

    /* Don't attempt to display the icon if either dimension is zero. */
    if (!width || !height)
        return;

    /* Get ourselves a colorspace. */
    img_header->color_space = context->color_space;
    IL_AddRefToColorSpace(img_header->color_space);

    /* Allocate bits for the icon.  We don't cache icons created for the
       purpose of printing. */
    img_depth = img_header->color_space->pixmap_depth;
    img_header->widthBytes = (img_header->width * img_depth + 7) / 8;
    img_header->widthBytes = ROUNDUP(img_header->widthBytes, 4);

    /* Allocate memory for the image bits. */
    image.bits = calloc(img_header->widthBytes * img_header->height, 1);
    if (!image.bits) {
        IL_ReleaseColorSpace(img_header->color_space);
        return;
    }

    /* Get the icon data from the screen FE and draw the icon. */
    if (FE_GetPSIconData(icon_number, &image, NULL))
        xl_colorimage(context, x, y, width, height, &image, NULL);
    
    /* Clean up. */
    free(image.bits);
    IL_ReleaseColorSpace(img_header->color_space);
}

/*****************************************************************************/
/*                       End of Image Library callbacks                      */
/*****************************************************************************/





