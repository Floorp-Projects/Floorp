/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* Image.cpp: Class for managing all imaging rendering issues
 * Created: Radha Kulkarni <radha@netscape.com> 21-Aug-1998
 */


#include "il_util.h"
#include "xfe.h"
#include "View.h"
#include "Frame.h"
#include "Image.h"

#ifdef DEBUG_radha
#define D(x) x
#else
#define D(x)
#endif


extern "C" {
CL_Compositor * fe_create_compositor(MWContext *context);
}


XFE_Image::XFE_Image(XFE_Component * frame, char * imageURL, fe_colormap * cmap, Widget  baseWidget) 
{


  struct fe_MWContext_cons *cons;

  // Initializations 
  m_urlString = strdup(imageURL);
  completelyLoaded = False;
  badImage = False;
  m_imageContext = NULL;
  fec = NULL;
  cxtInitSucceeded = False;
  m_image = NULL;
  m_mask = NULL;
  m_frame = frame;
  m_badImage = 0;


  // Create new context
  m_imageContext = XP_NewContext();

  cons = XP_NEW_ZAP(struct fe_MWContext_cons);
  if (cons == NULL)
  {
	XP_FREE(m_imageContext);
    return;
  }

  cons->context = m_imageContext;
  cons->next = fe_all_MWContexts;
  fe_all_MWContexts = cons;
  XP_AddContextToList (m_imageContext);

  fec = XP_NEW_ZAP(fe_ContextData);
  if (fec == NULL)
	{
		XP_FREE(cons);
		XP_FREE(m_imageContext);
		return ;
	}


  if (m_imageContext && fec) 
  {
    m_imageContext->type = MWContextIcon;
    CONTEXT_DATA(m_imageContext) = fec;

    // Set up the image library callbacks
    CONTEXT_DATA(m_imageContext)->DisplayPixmap = (DisplayPixmapPtr)fe_DisplayPixmap;
    CONTEXT_DATA(m_imageContext)->NewPixmap = (NewPixmapPtr)NewPixmap;
    CONTEXT_DATA(m_imageContext)->ImageComplete = (ImageCompletePtr)ImageComplete;

   
    CONTEXT_WIDGET (m_imageContext)                 = baseWidget; 
    CONTEXT_DATA   (m_imageContext)->drawing_area   = baseWidget;
    CONTEXT_DATA   (m_imageContext)->colormap       = cmap;
	if (fec == NULL)
	{
		XP_FREE(cons);
		XP_FREE(m_imageContext);
		return ;
	}
 	fe_InitRemoteServer (XtDisplay (baseWidget));

    m_imageContext->funcs = fe_BuildDisplayFunctionTable();
    m_imageContext->convertPixX = m_imageContext->convertPixY = 1;
    m_imageContext->is_grid_cell = FALSE;
    m_imageContext->grid_parent = NULL;

    fe_InitIconColors(m_imageContext);

    // Use colors from prefs

    LO_Color *color;

    color = &fe_globalPrefs.links_color;
    CONTEXT_DATA(m_imageContext)->link_pixel = fe_GetPixel(m_imageContext, 
							   color->red, 
							   color->green, 
							   color->blue);

    color = &fe_globalPrefs.vlinks_color;
    CONTEXT_DATA(m_imageContext)->vlink_pixel = fe_GetPixel(m_imageContext, 
							    color->red, 
							    color->green, 
							    color->blue);

    color = &fe_globalPrefs.text_color;
    CONTEXT_DATA(m_imageContext)->default_fg_pixel =fe_GetPixel(m_imageContext,
								color->red, 
								color->green, 
								color->blue);

    color = &fe_globalPrefs.background_color;
    CONTEXT_DATA(m_imageContext)->default_bg_pixel =fe_GetPixel(m_imageContext,
								color->red, 
								color->green, 
								color->blue);


    SHIST_InitSession (m_imageContext);
    fe_load_default_font(m_imageContext);
    {
       Pixel unused_select_pixel;
       XmGetColors (XtScreen (baseWidget),
		    fe_cmap(m_imageContext),
		    CONTEXT_DATA (m_imageContext)->default_bg_pixel,
		    &(CONTEXT_DATA (m_imageContext)->fg_pixel),
		    &(CONTEXT_DATA (m_imageContext)->top_shadow_pixel),
		    &(CONTEXT_DATA (m_imageContext)->bottom_shadow_pixel),
		    &unused_select_pixel);
    }


    /* Add mapping between MWContext and frame */
    ViewGlue_addMapping((XFE_Frame *)m_frame, m_imageContext);

    /* Initialize the Imagelib callbacks, Imagecontexts, 
     * imageobserver and group contexts
     */
    if (!fe_init_image_callbacks(m_imageContext))
    {
       cxtInitSucceeded = False;
       return;
    }

    fe_InitColormap (m_imageContext);
    cxtInitSucceeded = True;
    
    XtRealizeWidget(baseWidget);
    m_imageContext->compositor = fe_create_compositor(m_imageContext);
  }
  else
  {
    cxtInitSucceeded = False;
    return;
  }

}


XFE_Image::~XFE_Image(void)
{

  free(m_urlString);

  // Should I destroy the m_mask and m_image and the clientdata inside them?
  // Destroy the Image groupcontexts and observers
  if (m_imageContext)
  {
      PRBool observer_removed_p;

      if (m_imageContext->color_space) 
      {
         D(printf("Deleting image color spaces\n");)
	     IL_ReleaseColorSpace(m_imageContext->color_space);
          m_imageContext->color_space = NULL;
      }

      /* Destroy the image group context after removing the image group 
       * observer
       */
      if (m_imageContext->img_cx) {
           D(printf("Deleting the image group observer and context\n");)
            observer_removed_p =
                IL_RemoveGroupObserver(m_imageContext->img_cx,
                                       fe_ImageGroupObserver,
                                       (void *)m_imageContext);

			IL_DestroyGroupContext(m_imageContext->img_cx);
			m_imageContext->img_cx = NULL;
      }
  }  /* m_imageContext */
  free (CONTEXT_DATA (m_imageContext));
  free (m_imageContext);

}


/* returns if the image has been completely loaded */
PRBool
XFE_Image::isImageLoaded(void)
{
     return (completelyLoaded);

}


/* return the image pixmap */
Pixmap
XFE_Image::getPixmap(void)
{
   if (m_image)
     return (Pixmap)(((fe_PixmapClientData *)(m_image->client_data))->pixmap);
   return (Pixmap)NULL;
}

/* Return the image mask */
Pixmap
XFE_Image::getMask(void)
{
    if (m_mask)
       return (Pixmap)(((fe_PixmapClientData *)(m_mask->client_data))->pixmap);
    return (Pixmap)NULL;

}

/* Get image width */
PRInt32
XFE_Image::getImageWidth(void)
{
  return(imageWidth);
}

/* Get image height */
PRInt32
XFE_Image::getImageHeight(void)
{
  return(imageHeight);
}


/* Call NET_GetURL() to fetch the image */
void 
XFE_Image::loadImage(void)
{
   if (cxtInitSucceeded)
     NET_GetURL(NET_CreateURLStruct(m_urlString, NET_DONT_RELOAD),
                FO_CACHE_AND_PRESENT, m_imageContext,
                getURLExit_cb);
}

/*static*/ void
XFE_Image::getURLExit_cb(URL_Struct *pUrl, int iStatus, MWContext *pContext)
{
	//	Report any errors.
	if(iStatus < 0 && pUrl->error_msg != NULL)	
	{
         printf("Couldn't load image. Need to put the bad image up\n");
	}

    XP_FREE(pUrl);
}







///////////////////////////////////////////////////////////////////////////
//               ImageLibrary Callbacks                                  //
///////////////////////////////////////////////////////////////////////////

// The XFE handle to the image library callback -IMGCB_DisplayPixmap
extern "C" void 
fe_DisplayPixmap(MWContext * context, IL_Pixmap * image, IL_Pixmap * mask, PRInt32 width, PRInt32 height)
{

   XFE_Frame *    frameHandle=(XFE_Frame *)NULL;
   /* Get the handle to the frame from the context */
    
   if (context)
       frameHandle = (XFE_Frame *)ViewGlue_getFrame(context);
   if (!frameHandle)
     return;

   /* Check the context type. If it is anything other than MWContextIcon
    * let it do the regular image processing. If it is MWContextIcon,
    * Look for a NavCenterVIew in th frame and let the view handle the
    * pixmap display
    */

   if (context->type == MWContextIcon)
   {
       /* If the context type is MWContextIcon, get a handle to
        * NavCenterView and let it handle pixmap
        */
       XFE_View * navCenterView = XFE_View::getNavCenterView(frameHandle->getView());
       if (navCenterView)
       {
           Widget buttonWidget = CONTEXT_WIDGET(context);
           navCenterView->handleDisplayPixmap(buttonWidget, image, mask, width, height);
       }
            
   }

}  /* DisplayPixmap */



// The XFE handle to the image library callback _IMGCB_NewPixmap
/*static*/ void 
XFE_Image::NewPixmap(MWContext * context, IL_Pixmap * image, Boolean mask)
{

   XFE_Frame *    frameHandle=(XFE_Frame *)NULL;
   /* Get the handle to the frame from the context */
    
   if (context)
       frameHandle = ViewGlue_getFrame(context);

   /* Check the context type. If it is anything other than MWContextIcon
    * simply return, 'coz the frames don't have any processing for the
    * the NewPixmap callback.  If it is MWContextIcon,
    * Look for a NavCenterView in th frame and let the view handle it.
    */

   if (frameHandle)
   {
        if (context->type == MWContextIcon)
        {
            /* If the context type is MWContextIcon, get a handle to          
             * NavCenterView and let it handle pixmap
             */
            XFE_View * navCenterView = XFE_View::getNavCenterView(frameHandle->getView());
            if (navCenterView)
            {
                Widget buttonWidget = CONTEXT_WIDGET(context);
                navCenterView->handleNewPixmap(buttonWidget, image, mask);
            }
            

        }
   }
}   /* NewPixmap */


// The XFE handle to the image library callback _IMGCB_ImageComplete
/*static*/ void
XFE_Image::ImageComplete(MWContext * context, IL_Pixmap * image)
{
   XFE_Frame *    frameHandle=(XFE_Frame *)NULL;
   /* Get the handle to the frame from the context */
    
   if (context)
       frameHandle = (XFE_Frame *)ViewGlue_getFrame(context);

   /* Check the context type. If it is anything other than MWContextIcon
    * simply return, 'coz the frames don't have any processing for the
    * the NewPixmap callback.  If it is MWContextIcon,
    * Look for a NavCenterView in th frame and let the view handle it.
    */

   if (frameHandle)
   {
        if (context->type == MWContextIcon)
        {
            /* If the context type is MWContextIcon, let the NavCenterView
             * handle it
             */
            XFE_View * navCenterView = XFE_View::getNavCenterView(frameHandle->getView());
            if (navCenterView)
            {
                Widget buttonWidget = CONTEXT_WIDGET(context);
                navCenterView->handleImageComplete(buttonWidget, image);
            }
          }
   }

}  /* ImageComplete  */

#ifdef UNDEF

// The actual XFE function that renders image on a HTML area
/*static*/ void
XFE_Image::DisplayImage(MWContext * context, 
                        IL_Pixmap * image, IL_Pixmap * mask,
                        PRInt32 x,         PRInt32 y,
                        PRInt32 x_offset,  PRInt32 y_offset,
                        PRInt32 width,     PRInt32 height)
{

    int32 img_x_offset, img_y_offset;   /* Offset of image in drawable. */
    int32 rect_x_offset, rect_y_offset; /* Offset of update rect in
                                           drawable. */
    NI_PixmapHeader *img_header = &image->header;
    uint32 img_width = img_header->width;     /* Image width. */
    uint32 img_height = img_header->height;   /* Image height. */

    Widget widget = CONTEXT_WIDGET(context);
    fe_Drawable *fe_drawable = CONTEXT_DATA(context)->drawable;
    Drawable drawable = fe_drawable->xdrawable;
    Display *dpy = XtDisplay(widget);
    Pixmap img_x_pixmap, mask_x_pixmap=0;
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
                                           (Region)fe_drawable->clip_region);
            
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
                                                 (Region)fe_drawable->clip_region);
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
                                    (Region)fe_drawable->clip_region);
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
                                    &gcv, (Region)fe_drawable->clip_region);
            else
                gc = fe_GetGCfromDW(dpy, drawable, GCFunction, &gcv, NULL);
 
            /* Draw the image (opaque and non-tiled.) */
            XCopyArea (dpy, img_x_pixmap, drawable, gc, x_offset,
                       y_offset, width, height, rect_x_offset, rect_y_offset);
        }
    }


}  /* displayImage */


#endif  /* UNDEF  */
