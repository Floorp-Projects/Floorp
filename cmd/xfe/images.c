/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* */
/* images.c --- X front-end stuff dealing with images
   Created: Jamie Zawinski <jwz@netscape.com>.
 */


#define JMC_INIT_IMGCB_ID
#include "mozilla.h"
#include "xfe.h"

#include "libimg.h"             /* Image Library public API. */
#include "il_util.h"            /* Colormap/Colorspace API. */
#include "prtypes.h"

#ifdef EDITOR
#include "xeditor.h"
#endif

#define GRAYSCALE_WORKS

#define PARTIAL_IMAGE_BORDER_WIDTH 2

/* We set this flag when we're running on a visual of a depth that we
   know the image library won't support at all. */
Boolean fe_ImagesCantWork = False;

typedef struct fe_PixmapClientData {
    Pixmap pixmap;
    Display *dpy;
} fe_PixmapClientData;

/*
 * Layout has moved this image to a new location, and it may get
 * moved again before it is next displayed, so set valid to
 * false so its final new coords will be saved when next it is displayed.
 */
void
FE_ShiftImage (MWContext *context, LO_ImageStruct *lo_image)
{
                                /* XXXM12N Obsolete. */
}

/* Delaying images. */
void
fe_LoadDelayedImage (MWContext *context, const char *url)
{
  /* If autoload images is on and there are no delayed images, then there is
     nothing to be done. */
  if (fe_globalPrefs.autoload_images_p &&
      !CONTEXT_DATA (context)->delayed_images_p)
      return;

  if (url == FORCE_LOAD_ALL_IMAGES)
      LO_SetForceLoadImage(NULL, TRUE);
  else
      LO_SetForceLoadImage((char *)url, FALSE);
  CONTEXT_DATA (context)->delayed_images_p = False;

  fe_ReLayout (context, NET_DONT_RELOAD);
}

void
fe_LoadDelayedImages (MWContext *context)
{
  fe_LoadDelayedImage (context, FORCE_LOAD_ALL_IMAGES);
}

#ifdef EDITOR
void
FE_ClearBackgroundImage(MWContext* context)
{
                                /* XXXM12N Obsolete. */
}
#endif /*EDITOR*/


/* #define DEBUG_GROUP_OBSERVER */

/* Image group observer callback. */
void
fe_ImageGroupObserver(XP_Observable observable, XP_ObservableMsg message,
                 void *message_data, void *closure)
{
    IL_GroupMessageData *data = (IL_GroupMessageData*)message_data;
    MWContext *context = (MWContext *)closure;

    XP_ASSERT(context == data->display_context);
    XP_ASSERT(context->img_cx == data->image_context);

    switch (message) {
    case IL_STARTED_LOADING:
        CONTEXT_DATA(context)->loading_images_p = True;
        break;

    case IL_ABORTED_LOADING:
        if (!CONTEXT_DATA (context)->delayed_images_p &&
            !fe_ImagesCantWork) {
            CONTEXT_DATA (context)->delayed_images_p = True;
        }
    break;

    case IL_FINISHED_LOADING:
        /* Note: This means that all images currently in the context have
           finished loading.  If network activity has not ceased, then
           new images could start to load in the image context. */
        CONTEXT_DATA(context)->loading_images_p = False;
        break;

    case IL_STARTED_LOOPING:
        CONTEXT_DATA(context)->looping_images_p = True;
        break;

    case IL_FINISHED_LOOPING:
        /* Note: This means that all loaded images currently in the context
           have finished looping.  If network activity has not ceased, then
           new images could start to loop in the image context. */
        CONTEXT_DATA(context)->looping_images_p = False;
        break;
        
    default:
        break;
    }
	FE_UpdateStopState(context);
}

/*****************************************************************************/
/*                       Image Library callbacks                             */
/*****************************************************************************/

#define HOWMANY(x, r)     (((x) + ((r) - 1)) / (r))
#define ROUNDUP(x, r)     (HOWMANY(x, r) * (r))

JMC_PUBLIC_API(void)
_IMGCB_init(struct IMGCB* self, JMCException* *exception)
{
    /* Nothing to be done here. */
}

extern JMC_PUBLIC_API(void*)
_IMGCB_getBackwardCompatibleInterface(struct IMGCB* self,
                                      const JMCInterfaceID* iid,
                                      JMCException* *exception)
{
    return NULL;                /* Nothing to be done here. */
}


/**************************** Pixmap creation ********************************/
JMC_PUBLIC_API(void)
_IMGCB_NewPixmap(IMGCB* img_cb, jint op, void *dpy_cx, jint width, jint height,
          IL_Pixmap *image, IL_Pixmap *mask)
{
    uint8 img_depth;
    NI_PixmapHeader *img_header = &image->header;
    NI_PixmapHeader *mask_header = mask ? &mask->header : NULL;
    MWContext *context = (MWContext *)dpy_cx; /* XXXM12N This should be the
                                                 FE's display context. */
    Widget widget = CONTEXT_WIDGET(context);
    Display *dpy = XtDisplay(widget);
    Visual *visual = 0;
    Window window;
    unsigned int visual_depth;
    unsigned int pixmap_depth;
    Pixmap img_x_pixmap, mask_x_pixmap = NULL;
    fe_PixmapClientData *img_client_data, *mask_client_data = NULL;

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

    /* Get the required display parameters */
    XtVaGetValues (widget, XmNvisual, &visual, 0);
    if (!visual) visual = fe_globalData.default_visual;
    window = XtWindow (CONTEXT_DATA (context)->drawing_area);
    visual_depth = fe_VisualDepth (dpy, visual);
    pixmap_depth = fe_VisualPixmapDepth (dpy, visual);

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
}


/**************************** Pixmap update **********************************/
JMC_PUBLIC_API(void)
_IMGCB_UpdatePixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* pixmap,
             jint x_offset, jint y_offset, jint width, jint height)
{
    uint32 widthBytes;
    MWContext *context = (MWContext *)dpy_cx; /* XXX This should be the FE's
                                                 display context. */
    Widget widget;
    Display *dpy;
    Visual *visual = 0;
    Window window;
    unsigned int visual_depth;
    unsigned int visual_pixmap_depth; /* Pixmap depth supported by visual. */
    unsigned int pixmap_depth;  /* Depth of the IL_Pixmap. */
    Pixmap x_pixmap;
    fe_PixmapClientData *pixmap_client_data;
    XImage *x_image;
    IL_ColorSpace *color_space = pixmap->header.color_space;
    char *bits;
    GC gc;
    XGCValues gcv;
    XP_Bool cached_gc;
    
    if (!context)
        return;
    widget = CONTEXT_WIDGET(context);
    dpy = XtDisplay(widget);

    /* Check for zero dimensions. */
    if (width == 0 || height == 0)
        return;

    /* Get the required display parameters */
    XtVaGetValues (widget, XmNvisual, &visual, 0);
    if (!visual) visual = fe_globalData.default_visual;
    window = XtWindow (CONTEXT_DATA (context)->drawing_area);
    visual_depth = fe_VisualDepth (dpy, visual);
    visual_pixmap_depth = fe_VisualPixmapDepth (dpy, visual);

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
    x_pixmap = pixmap_client_data->pixmap;

    memset(&gcv, ~0, sizeof(XGCValues));
    gcv.function = GXcopy;

    if (pixmap_depth == visual_pixmap_depth) {
        /* Get the GC out of the GC cache. */
        gc = fe_GetGC(widget, GCFunction, &gcv);
        cached_gc = TRUE;
    }
    else {
        /* The pixmap_depth does not correspond to that of the GCs in the
           cache, so create a new GC. */
        gc = XCreateGC(dpy, x_pixmap, GCFunction, &gcv);
        cached_gc = FALSE;
    }

    XPutImage(dpy, x_pixmap, gc, x_image, x_offset, 0, x_offset,
              y_offset, width, height);

    if (!cached_gc)
        XFreeGC(dpy, gc);
    x_image->data = 0;          /* Don't free the IL_Pixmap's bits. */
    XDestroyImage(x_image);
}


/************************** Pixmap memory management *************************/
JMC_PUBLIC_API(void)
_IMGCB_ControlPixmapBits(IMGCB* img_cb, jint op, void* dpy_cx,
                         IL_Pixmap* pixmap, IL_PixmapControl message)
{
}


/**************************** Pixmap destruction *****************************/
/* XXXM12N The dpy_cx argument is not used in DestroyPixmap and should be
   removed. */
JMC_PUBLIC_API(void)
_IMGCB_DestroyPixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* pixmap)
{
    fe_PixmapClientData *pixmap_client_data = pixmap->client_data;
    Display *dpy;
    Pixmap x_pixmap;

    if (!pixmap_client_data)
        return;

    dpy = pixmap_client_data->dpy;
    x_pixmap = pixmap_client_data->pixmap;
 
    if (x_pixmap) {
        XFreePixmap(dpy, x_pixmap);
        pixmap->client_data = NULL;
    }

    if (pixmap->bits) {
        free(pixmap->bits);
        pixmap->bits = NULL;
    }
}


/**************************** Pixmap display *********************************/
typedef struct 
{
    Pixmap x_pixmap;
    GC gc;
    Display *dpy;
    int32 x_clip_offset, y_clip_offset;
    XP_Rect *fill_rect;
} fe_FillTiledPixmapClosure;

static void
fe_FillTiledPixmapRectFunc(void *closure, XP_Rect *clip_rect)
{
    fe_FillTiledPixmapClosure *fill_closure =
        (fe_FillTiledPixmapClosure*)closure;
    Display *dpy = fill_closure->dpy; 
    Pixmap x_pixmap = fill_closure->x_pixmap;
    GC gc = fill_closure->gc;
    int32 x_clip_offset = fill_closure->x_clip_offset;
    int32 y_clip_offset = fill_closure->y_clip_offset;
    XP_Rect *fill_rect = fill_closure->fill_rect;
    XP_Rect sub_rect;
    uint32 sub_rect_width, sub_rect_height;

    /* Convert the clip rect's coordinates to be relative to the pixmap's
       origin. */
    XP_OffsetRect(clip_rect, x_clip_offset, y_clip_offset);
    XP_IntersectRect(clip_rect, fill_rect, &sub_rect);

    /* Compute the width and height of the actual area to be drawn.  If either
       dimension is zero, we have nothing to do. */
    sub_rect_width = sub_rect.right - sub_rect.left;
    sub_rect_height = sub_rect.bottom - sub_rect.top;
    if (!sub_rect_width || !sub_rect_height)
        return;

    /* Draw a sub_rect of the temporary mask. */
    XFillRectangle(dpy, x_pixmap, gc, sub_rect.left, sub_rect.top,
                   sub_rect_width, sub_rect_height);
}

/* Generate a tiled mask for the case where the fe_Drawable has a clip
   region.  Offset arguments are wrt the origin of the tiled mask. */
static Pixmap
fe_TiledMaskWithClipRegion(Display *dpy, Drawable drawable,
                           Pixmap mask_x_pixmap,
                           unsigned int width, unsigned int height,
                           int x_tile_offset, int y_tile_offset,
                           int x_clip_offset, int y_clip_offset,
                           Region clip_region)
{
    GC gc;
    XGCValues gcv;
    unsigned long flags;
    XP_Rect fill_rect;
    Pixmap ret_x_pixmap;
    fe_FillTiledPixmapClosure fill_closure;

    memset (&gcv, ~0, sizeof (XGCValues));

    ret_x_pixmap = XCreatePixmap(dpy, drawable, width, height, 1);
    
    /* The pixmap must be cleared since it may cover areas which do not
       belong to the clip region. */
    gcv.function = GXclear;
    gc = XCreateGC (dpy, ret_x_pixmap, GCFunction, &gcv);
    XFillRectangle(dpy, ret_x_pixmap, gc, 0, 0, width, height);

    /* Set up the GC. */
    gcv.function = GXcopy;
    gcv.fill_style = FillTiled;
    gcv.tile = mask_x_pixmap;
    gcv.ts_x_origin = x_tile_offset;
    gcv.ts_y_origin = y_tile_offset;
    flags = GCFunction | GCFillStyle | GCTile |
        GCTileStipXOrigin | GCTileStipYOrigin;
    XChangeGC(dpy, gc, flags, &gcv);

    /* Tile the mask_pixmap into the tiled_mask_pixmap, but only do so for
       areas in the clip region's rectangle list. */
    fill_rect.left = 0;
    fill_rect.top = 0;
    fill_rect.right = width;
    fill_rect.bottom = height;
    fill_closure.fill_rect = &fill_rect;
    fill_closure.x_pixmap = ret_x_pixmap;
    fill_closure.gc = gc;
    fill_closure.dpy = dpy;
    fill_closure.x_clip_offset = x_clip_offset;
    fill_closure.y_clip_offset = y_clip_offset;
    FE_ForEachRectInRegion(clip_region, fe_FillTiledPixmapRectFunc,
                           &fill_closure);

    /* Clean up. */
    XFreeGC(dpy, gc);

    return ret_x_pixmap;
}


typedef struct 
{
    Pixmap img_x_pixmap, mask_x_pixmap;
    Drawable drawable;
    GC gc, mask_gc;
    Display *dpy;
    int32 img_x_offset, img_y_offset;
    XP_Rect *fill_rect;
} fe_FillPixmapClosure;

static void
fe_FillPixmapRectFunc(void *closure, XP_Rect *clip_rect)
{
    fe_FillPixmapClosure *fill_closure = (fe_FillPixmapClosure*)closure;
    Display *dpy = fill_closure->dpy; 
    Pixmap img_x_pixmap = fill_closure->img_x_pixmap;
    Pixmap mask_x_pixmap = fill_closure->mask_x_pixmap;
    Drawable drawable = fill_closure->drawable;
    GC gc = fill_closure->gc;
    GC mask_gc = fill_closure->mask_gc;
    int32 img_x_offset = fill_closure->img_x_offset;
    int32 img_y_offset = fill_closure->img_y_offset;
    XP_Rect *fill_rect = fill_closure->fill_rect;
    XP_Rect sub_rect;
    Pixmap tmp_pixmap;
    XGCValues gcv;
    unsigned long flags;
    int32 sub_rect_x_offset, sub_rect_y_offset; /* Offsets wrt image origin. */
    uint32 sub_rect_width, sub_rect_height;

    /* Rectangle coordinates are all relative to the drawable origin. */
    XP_IntersectRect(clip_rect, fill_rect, &sub_rect);

    /* Compute the width and height of the actual area to be drawn.  If either
       dimension is zero, we have nothing to do. */
    sub_rect_width = sub_rect.right - sub_rect.left;
    sub_rect_height = sub_rect.bottom - sub_rect.top;
    if (!sub_rect_width || !sub_rect_height)
        return;

    /* Compute the offsets, wrt the image origin, of the area to be drawn. */
    sub_rect_x_offset = sub_rect.left - img_x_offset;
    sub_rect_y_offset = sub_rect.top - img_y_offset;

    /* Create a temporary mask pixmap which is the size of the area to be
       drawn. */
    tmp_pixmap = XCreatePixmap(dpy, drawable, sub_rect_width, sub_rect_height,
                               1);
    XCopyArea(dpy, mask_x_pixmap, tmp_pixmap, mask_gc, sub_rect_x_offset,
              sub_rect_y_offset, sub_rect_width, sub_rect_height, 0, 0);

    /* Use the temporary mask as the GC's clip mask, instead of using the
       entire image mask.  For complicated masks, this has a significant
       performance benefit on X servers which convert clip_masks to
       rectangle lists. */
    gcv.clip_mask = tmp_pixmap;
    gcv.clip_x_origin = sub_rect.left;
    gcv.clip_y_origin = sub_rect.top;
    flags = GCClipMask | GCClipXOrigin | GCClipYOrigin;
    XChangeGC(dpy, gc, flags, &gcv);

    /* Draw a sub_rect of the image. */
    XCopyArea(dpy, img_x_pixmap, drawable, gc, sub_rect_x_offset,
              sub_rect_y_offset, sub_rect_width, sub_rect_height,
              sub_rect.left, sub_rect.top);

    /* Clean up. */
    XFreePixmap(dpy, tmp_pixmap);
}

/* Draw the masked image for each rectangle in the compositor's clip region.
   x_offset and y_offset are wrt the image origin, while rect_x_offset and
   rect_y_offset are wrt the drawable origin. */
static void
fe_DrawMaskedImageWithClipRegion(Display *dpy, Drawable drawable,
                                 Pixmap img_x_pixmap, Pixmap mask_x_pixmap,
                                 unsigned int width, unsigned int height,
                                 int img_x_offset, int img_y_offset,
                                 int x_offset, int y_offset,
                                 Region clip_region)
{
    GC gc, mask_gc;
    XGCValues gcv;
    XP_Rect fill_rect;
    fe_FillPixmapClosure fill_closure;

    memset (&gcv, ~0, sizeof (XGCValues));

    /* Set up the GCs. */
    gcv.function = GXcopy;
    gc = XCreateGC(dpy, drawable, GCFunction, &gcv);
    mask_gc = XCreateGC(dpy, mask_x_pixmap, GCFunction, &gcv);

    /* Draw the masked image into the drawable, but only do so for areas in
       the clip region's rectangle list. */
    fill_rect.left = img_x_offset + x_offset;
    fill_rect.top = img_y_offset + y_offset;
    fill_rect.right = fill_rect.left + width;
    fill_rect.bottom = fill_rect.top + height;
    fill_closure.fill_rect = &fill_rect;
    fill_closure.img_x_pixmap = img_x_pixmap;
    fill_closure.mask_x_pixmap = mask_x_pixmap;
    fill_closure.drawable = drawable;
    fill_closure.gc = gc;
    fill_closure.mask_gc = mask_gc;
    fill_closure.dpy = dpy;
    fill_closure.img_x_offset = img_x_offset;
    fill_closure.img_y_offset = img_y_offset;
    FE_ForEachRectInRegion(clip_region, fe_FillPixmapRectFunc, &fill_closure);

    /* Clean up. */
    XFreeGC(dpy, gc);
    XFreeGC(dpy, mask_gc);
}


JMC_PUBLIC_API(void)
_IMGCB_DisplayPixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* image,
                     IL_Pixmap* mask, jint x, jint y, jint x_offset,
                     jint y_offset, jint width, jint height, jint req_w, jint req_h)
{
    int32 img_x_offset, img_y_offset;   /* Offset of image in drawable. */
    int32 rect_x_offset, rect_y_offset; /* Offset of update rect in
                                           drawable. */
    NI_PixmapHeader *img_header = &image->header;
    uint32 img_width = img_header->width;     /* Image width. */
    uint32 img_height = img_header->height;   /* Image height. */
    MWContext *context = (MWContext *)dpy_cx; /* XXX This should be the FE's
                                                 display context. */
    Widget widget = CONTEXT_WIDGET(context);
    fe_Drawable *fe_drawable = CONTEXT_DATA(context)->drawable;
    Drawable drawable = fe_drawable->xdrawable;
    Display *dpy = XtDisplay(widget);
    Pixmap img_x_pixmap, mask_x_pixmap;
    fe_PixmapClientData *img_client_data, *mask_client_data;
    GC gc;
    XGCValues gcv;
    unsigned long flags;
    XP_Bool tiling_required = FALSE;
    
    /* Check for zero display area. */
    if (width == 0 || height == 0)
        return;

    /* Retrieve the server pixmaps. */
    img_client_data = (fe_PixmapClientData *)image->client_data;
    if (!img_client_data)
        return;
    img_x_pixmap = img_client_data->pixmap;
    if (!img_x_pixmap)
        return;
    if (mask) {
        mask_client_data = (fe_PixmapClientData *)mask->client_data;
        mask_x_pixmap = mask_client_data->pixmap;
    }

    /* Determine whether tiling is required. */
    if ((x_offset + width > img_width) || (y_offset + height > img_height))
        tiling_required = TRUE;

    /* Compute the offset into the drawable of the image origin. */
    img_x_offset = x - CONTEXT_DATA(context)->document_x +
        fe_drawable->x_origin;
    img_y_offset = y - CONTEXT_DATA(context)->document_y +
        fe_drawable->y_origin;

    /* Compute the offset into the drawable for the area to be drawn. */
    rect_x_offset = img_x_offset + x_offset;
    rect_y_offset = img_y_offset + y_offset;

    /* Do the actual drawing.  There are several cases to be dealt with:
       transparent vs non-transparent, tiled vs non-tiled and clipped by
       compositor's clip region vs not clipped. */
    memset(&gcv, ~0, sizeof (XGCValues));
    if (mask) {                 /* Image is transparent. */
        if (tiling_required) {
            /* Offsets are measured wrt the origin of the tiled mask to
               be generated. */
            int x_tile_offset = img_x_offset - rect_x_offset;
            int y_tile_offset = img_y_offset - rect_y_offset;
            Pixmap tmp_pixmap = 0;

            /* Create the mask by tiling the mask_x_pixmap and computing
               the intersection with the compositor's clip region. */
            tmp_pixmap =
                fe_TiledMaskWithClipRegion(dpy, drawable, mask_x_pixmap,
                                           width, height, x_tile_offset,
                                           y_tile_offset, -rect_x_offset,
                                           -rect_y_offset,
                                           fe_drawable->clip_region);
            
            /* Create the GC.  Don't attempt to get a GC from the GC cache
               because we are using a temporary mask pixmap. */
            gcv.fill_style = FillTiled;
            gcv.tile = img_x_pixmap;
            gcv.ts_x_origin = img_x_offset;
            gcv.ts_y_origin = img_y_offset;
            gcv.clip_mask = tmp_pixmap;
            gcv.clip_x_origin = rect_x_offset;
            gcv.clip_y_origin = rect_y_offset;
            flags = GCFillStyle | GCTile | GCTileStipXOrigin |
                GCTileStipYOrigin | GCClipMask | GCClipXOrigin | GCClipYOrigin;
            gc = XCreateGC(dpy, drawable, flags, &gcv);

            /* Draw the image (transparent and tiled.) */
            XFillRectangle (dpy, drawable, gc, rect_x_offset, rect_y_offset,
                            width, height);

            /* Clean up. */
            XFreeGC(dpy, gc);
            XFreePixmap(dpy, tmp_pixmap);
        }
        else {                  /* Tiling not required. */
            if (fe_drawable->clip_region) {
                
                /* Draw the image (transparent, non-tiled and with
                   clip_region.)  x_offset and y_offset are wrt the image
                   origin, while rect_x_offset and rect_y_offset are wrt the
                   drawable origin. */
                fe_DrawMaskedImageWithClipRegion(dpy, drawable, img_x_pixmap,
                                                 mask_x_pixmap, width, height,
                                                 img_x_offset, img_y_offset,
                                                 x_offset, y_offset,
                                                 fe_drawable->clip_region);
            }
            else {              /* No clip region. */
                /* XXX transparent, non-tiled and no clip_region. */
            }
        }
    }
    else {                      /* Image is not transparent. */
        if (tiling_required) {
            /* Get the GC from the GC cache.  If the compositor has given us
               a clip region, then the GC must have a matching clip mask. */
            gcv.fill_style = FillTiled;
            gcv.tile = img_x_pixmap;
            gcv.ts_x_origin = img_x_offset;
            gcv.ts_y_origin = img_y_offset;
            flags = GCFillStyle | GCTile | GCTileStipXOrigin |
                GCTileStipYOrigin;
            if (fe_drawable->clip_region)
                gc = fe_GetGCfromDW(dpy, drawable, flags, &gcv,
                                    fe_drawable->clip_region);
            else
                gc = fe_GetGCfromDW(dpy, drawable, flags, &gcv, NULL);

            /* Draw the image (opaque and tiled.) */
            XFillRectangle (dpy, drawable, gc, rect_x_offset, rect_y_offset,
                            width, height);  
        }
        else {                  /* Tiling not required. */
            /* Get the GC from the GC cache.  If the compositor has given us
               a clip region, then the GC must have a matching clip mask. */
            gcv.function = GXcopy;
            if (fe_drawable->clip_region)
                gc = fe_GetGCfromDW(dpy, drawable, GCFunction,
                                    &gcv, fe_drawable->clip_region);
            else
                gc = fe_GetGCfromDW(dpy, drawable, GCFunction, &gcv, NULL);
 
            /* Draw the image (opaque and non-tiled.) */
            XCopyArea (dpy, img_x_pixmap, drawable, gc, x_offset,
                       y_offset, width, height, rect_x_offset, rect_y_offset);
        }
    }
}

/* Mocha image group observer callback. */
void
FE_MochaImageGroupObserver(XP_Observable observable, XP_ObservableMsg message,
                           void *message_data, void *closure)
{
}

/*****************************************************************************/
/*                       End of Image Library callbacks                      */
/*****************************************************************************/
