/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   mkicons.c --- converting transparent GIFs to embeddable XImage data.
   Created: Jamie Zawinski <jwz@netscape.com>, 17-Aug-95.
   (Danger.  Here be monsters.)
 */

#define M12N

#define JMC_INIT_IMGCB_ID
#define JMC_INIT_IMGCBIF_ID
#ifndef NSPR20
#include "prosdep.h"
#else
#include "prtypes.h"
#endif

static char* header_comment =	
"/*\n"
" * The XFE macro is used to separate the icons used in\n"
" * the xfe from icons that are used only in shared objects.\n"
" * They are both included in icondata.c because they need\n"
" * to refer to the same color map, and it's easier to keep them\n"
" * in sync this way.  Icons that are compiled into shared objects\n"
" * but not the Communicator are preceded by an underscore.\n"
" * Their filename is of the form: _MACRO_NAME.filename.gif\n"
" * When the module that wants that icon compiles icondata.c, it\n"
" * will define MACRO_NAME, but not XFE.\n"
" * For an example, see images/_POLARIS.SplashPro.gif, which is used by\n"
" * the Polaris shared library, but isn't compiled into the Communicator.\n"
" *\n"
" * Questions to malmer@netscape.com.\n"
" */\n\n";

#include "if.h"                 /* Image Library private header. */
#include "libimg.h"             /* Image Library public API. */
#include "il_util.h"            /* Colormap/colorspace API. */
#include "il_strm.h"            /* Image Library stream converters. */

#include "structs.h"            /* For MWContext. */

#define MAX_ANIMS 10
#define MAX_IMAGES 1000

/* =========================================================================
   All this junk is just to get it to link.
   =========================================================================
 */
void * FE_SetTimeout(TimeoutCallbackFunction func, void * closure,
                     uint32 msecs) { return 0; }
void FE_ClearTimeout(void *timer_id) {}

void lo_view_title(MWContext *context, char *title_str) {}

#ifdef MOCHA
void LM_SendImageEvent(MWContext *context, LO_ImageStruct *image_data,
                       void * event) { return;}
#endif

int NET_URL_Type(const char *URL) {return 0;}
    
void XP_Trace(const char * format, ...) {}

int32 NET_GetMemoryCacheSize(void) { return 1000000; }
int32 NET_GetMaxMemoryCacheSize(void) { return 1000000; }
void NET_FreeURLStruct(URL_Struct * URL_s) {}
int NET_InterruptWindow(MWContext * window_id) {return 0;}
URL_Struct *NET_CreateURLStruct(const char *url, NET_ReloadMethod reload) { return 0; }
History_entry *  SHIST_GetCurrent(History * hist) { return 0; }
int NET_GetURL (URL_Struct * URL_s, FO_Present_Types output_format,
		MWContext * context, Net_GetUrlExitFunc* exit_routine) 
{ return -1; }
Bool LO_BlockedOnImage(MWContext *c, LO_ImageStruct *image) { return FALSE; }
Bool NET_IsURLInDiskCache(URL_Struct *URL_s) {return TRUE;}
XP_Bool NET_IsLocalFileURL(char *address) {return TRUE;}

NET_StreamClass * NET_StreamBuilder (FO_Present_Types  format_out,
				     URL_Struct *anchor, MWContext *window_id)
{ return 0; }

Bool NET_AreThereActiveConnectionsForWindow(MWContext * window_id)
{ return FALSE; }
Bool NET_AreThereStoppableConnectionsForWindow(MWContext * window_id)
{ return FALSE; }
void GH_UpdateGlobalHistory(URL_Struct * URL_s) { }
char * NET_EscapeHTML(const char * string) { return (char *)string; }

extern int il_first_write(NET_StreamClass *stream, const unsigned char *str, int32 len);

char *XP_GetBuiltinString(int16 i);

char *
XP_GetString(int16 i)
{
	return XP_GetBuiltinString(i);
}

Bool
NET_IsURLInMemCache(URL_Struct *URL_s)
{
	return FALSE;
}

/* =========================================================================
   Now it gets REALLY nasty.
   =========================================================================
 */

struct image {
  int width, height;
  char *color_bits;
  char *mono_bits;
  char *mask_bits;
};

int total_images = 0;
struct image images[MAX_IMAGES] = { { 0, }, };

IL_ColorMap global_cmap;
IL_ColorMap *src_cmap = NULL;   /* Colormap of the current source image. */
int src_cmap_num_colors = 0;    /* Total number of colors in source image's
                                   colormap. */
char *currentIconName = NULL;
XP_Bool haveSeenTransparentColor;
XP_Bool haveAddedTransparentColor = FALSE;

uint8 transparent_color[4] =
  { 255,   0, 255,   0 };

#ifdef DEBUG
/* Check for unused colors */
XP_Bool is_global_cmap_color_used[256];

#define NUM_DESIRED_COLORS 24
uint8 desired_colors[][4] = {
/*{   R,   G,   B, pad } */
  { 255, 255,   0,   0 },  /*  0 */
  { 255, 102,  51,   0 },  /*  1 */
  { 128, 128,   0,   0 },  /*  2 */
  {   0, 255,   0,   0 },  /*  3 */
  {   0, 128,   0,   0 },  /*  4 */
  {  66, 154, 167,   0 },  /*  5 */
  {   0, 128, 128,   0 },  /*  6 */
  {   0,  55,  60,   0 },  /*  7 */
  {   0, 255, 255,   0 },  /*  8 */
  {   0,   0, 255,   0 },  /*  9 */
  {   0,   0, 128,   0 },  /* 10 */
  { 153, 153, 255,   0 },  /* 11 */
  { 102, 102, 204,   0 },  /* 12 */
  {  51,  51, 102,   0 },  /* 13 */
  { 255,   0,   0,   0 },  /* 14 */
  { 128,   0,   0,   0 },  /* 15 */
  { 255, 102, 204,   0 },  /* 16 */
  { 153,   0, 102,   0 },  /* 17 */
  { 255, 255, 255,   0 },  /* 18 */
  { 192, 192, 192,   0 },  /* 19 */
  { 128, 128, 128,   0 },  /* 20 */
  {  34,  34,  34,   0 },  /* 21 */
  {   0,   0,   0,   0 },  /* 22 */
  {  26,  95, 103,   0 },  /* 23 */
};
#endif /* DEBUG_COLOR_PALLETE */


XP_Bool did_size = FALSE;
int column = 0;

int in_anim = 0;
int inactive_icon_p = 0;
int anim_frames[100] = { 0, };
char macro[256];
XP_Bool iconIsNotUsedInMozLite = FALSE;

XP_Bool sgi_p = FALSE;

static unsigned char *bitrev = 0;

static void
init_reverse_bits(void)
{
  if(!bitrev) 
    {
      int i, x, br;
      bitrev = (unsigned char *)XP_ALLOC(256);
      for(i=0; i<256; i++) 
        {
          br = 0;
          for(x=0; x<8; x++) 
            {
              br = br<<1;
              if(i&(1<<x))
                br |= 1;
            }
          bitrev[i] = br;
        }
    }
}

/*****************************************************************************/
/*                       Image Library callbacks                             */
/*****************************************************************************/

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


JMC_PUBLIC_API(void)
_IMGCB_NewPixmap(IMGCB* img_cb, jint op, void *dpy_cx, jint width, jint height,
                 IL_Pixmap *image, IL_Pixmap *mask)
{
    NI_PixmapHeader *img_header = &image->header;
    NI_PixmapHeader *mask_header = mask ? &mask->header : NULL;

    /* Allocate memory for the image bits, and for the mask bits (if
       required.) */
    image->bits = calloc(img_header->widthBytes * img_header->height, 1);
    if (!image->bits)
        return;
    if (mask) {
        mask->bits = calloc(mask_header->widthBytes * mask_header->height, 1);
        if (!mask->bits) {
            free(image->bits);
            image->bits = 0;
        }
    }
}


JMC_PUBLIC_API(void)
_IMGCB_UpdatePixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* pixmap,
             jint x_offset, jint y_offset, jint width, jint height)
{
}


JMC_PUBLIC_API(void)
_IMGCB_ControlPixmapBits(IMGCB* img_cb, jint op, void* dpy_cx,
                         IL_Pixmap* pixmap, IL_PixmapControl message)
{
}


JMC_PUBLIC_API(void)
_IMGCB_DestroyPixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* pixmap)
{
    if (pixmap->bits)
        free(pixmap->bits);
    pixmap->bits = NULL;
    
    if (pixmap->client_data)
        free(pixmap->client_data);
    pixmap->client_data = NULL;
}


/* The DisplayPixmap callback is used to write out the decoded icon data. */
JMC_PUBLIC_API(void)
_IMGCB_DisplayPixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* image,
                     IL_Pixmap* mask, jint x, jint y, jint x_offset,
                     jint y_offset, jint width, jint height)
{
  int i;
  int row_parity;
#ifdef DEBUG
  XP_Bool isColorOk; /* Sniff out undesired colors */
  int bestMatch, matchDiff, currDiff, rDiff, gDiff, bDiff;
#endif
  unsigned char *s, *m, *scanline, *end, *mask_scanline = NULL;
  NI_PixmapHeader *img_header = &image->header;
  NI_PixmapHeader *mask_header = &mask->header;
  uint32 pixmap_depth = img_header->color_space->pixmap_depth;
  
  fprintf (stdout, " %d, %d,\n", img_header->width, img_header->height);

  if (total_images >= MAX_IMAGES) {
	fprintf (stderr, 
			 "Total images, %d, greater than max allowed, %d.\n",
			 total_images, MAX_IMAGES);
	abort();
  }

  images[total_images].width = img_header->width;
  images[total_images].height = img_header->height;
  if (pixmap_depth == 1)
      images[total_images].mono_bits = image->bits;
  else
      images[total_images].color_bits = image->bits;
  if (mask)
      images[total_images].mask_bits = mask->bits;

  for (i = 0; i < src_cmap_num_colors; i++) 
	{
      int j;

	  /* Skip the transparent color */
	  if (src_cmap->map[i].red   != transparent_color[0] ||
		  src_cmap->map[i].green != transparent_color[1] ||
		  src_cmap->map[i].blue  != transparent_color[2])
		{
#ifdef DEBUG
		  /* fprintf (stderr, "%2i: %3d %3d %3d\n",i,
			   src_cmap->map[i].red,
			   src_cmap->map[i].green,
			   src_cmap->map[i].blue);
    	   */
		  matchDiff = 255001;  /* Larger than possible diff */
		  isColorOk = FALSE;
		  for (j = 0; j < NUM_DESIRED_COLORS; j++)
			{
			  if (src_cmap->map[i].red   == desired_colors[j][0] &&
				  src_cmap->map[i].green == desired_colors[j][1] &&
				  src_cmap->map[i].blue  == desired_colors[j][2])
				{
				  isColorOk = TRUE;
				  break;
				} 
			  else /* Find the closest match */
				{ 
				  rDiff = abs(src_cmap->map[i].red   - desired_colors[j][0]);
				  gDiff = abs(src_cmap->map[i].green - desired_colors[j][1]);
				  bDiff = abs(src_cmap->map[i].blue  - desired_colors[j][2]);
				  /* Computed the color difference based on the apparent
				   * luminance of the color components
				   */
				  currDiff = (rDiff * 299) + (gDiff * 587) + (bDiff * 114);
				  if (matchDiff > currDiff) 
					{
					  matchDiff = currDiff;
					  bestMatch = j;
					}
				}
			}
		  if (!isColorOk &&
			  (i == 0 ||
			   src_cmap->map[i].red   != src_cmap->map[i-1].red   ||
			   src_cmap->map[i].green != src_cmap->map[i-1].green ||
			   src_cmap->map[i].blue  != src_cmap->map[i-1].blue))
			{
			  fprintf(stderr,
					  "Bad color:  %3d %3d %3d   try  %3d %3d %3d  %s\n",
					  src_cmap->map[i].red,
					  src_cmap->map[i].green,
					  src_cmap->map[i].blue,
					  desired_colors[bestMatch][0],
					  desired_colors[bestMatch][1],
					  desired_colors[bestMatch][2],
					  /*matchDiff,*/
					  currentIconName);
			}
#endif /* DEBUG */

		  for (j = 0; j < global_cmap.num_colors; j++) {
			if (src_cmap->map[i].red   == global_cmap.map[j].red &&
				src_cmap->map[i].green == global_cmap.map[j].green &&
				src_cmap->map[i].blue  == global_cmap.map[j].blue)
			  goto FOUND;
		  }
		  global_cmap.map[global_cmap.num_colors].red   = src_cmap->map[i].red;
		  global_cmap.map[global_cmap.num_colors].green = src_cmap->map[i].green;
		  global_cmap.map[global_cmap.num_colors].blue  = src_cmap->map[i].blue;
		
		  global_cmap.num_colors++;
		  if (global_cmap.num_colors > 100) {
			fprintf(stderr, "Global color map exceeded 100 colors in %s\n",
					currentIconName);
			abort();
		  }
		FOUND:
		  ;
		}
	}


  if (pixmap_depth == 1)
    return;
  if (pixmap_depth != 32) {
	fprintf(stderr, "The pixmap_depth isn't 32 in %s.\n", currentIconName);
	abort();
  }

  /* Generate monochrome icon from color data. */
  scanline = image->bits;
  if (mask)
      mask_scanline = mask->bits;
  end = scanline + (img_header->widthBytes * img_header->height);
  row_parity = 0;
      
  fprintf (stdout, "\n /* Monochrome icon */\n (unsigned char*)\n \"");
  column = 2;
  while (scanline < end)
    {
      unsigned char *scanline_end = scanline + (img_header->width * 4);
      int luminance, pixel;
      int bit = 0;
      int byte = 0;

      row_parity ^= 1;
      for (m = mask_scanline, s = scanline; s < scanline_end; s += 4)
        {
          unsigned char r = s[3];
          unsigned char g = s[2];
          unsigned char b = s[1];

          if (column > 74)
            {
              fprintf (stdout, "\"\n \"");
              column = 2;
            }
          luminance = (0.299 * r) + (0.587 * g) + (0.114 * b);

          pixel =
            ((luminance < 128))                      ||
            ((r ==  66) && (g == 154) && (b == 167)); /* Magic: blue */
          byte |= pixel << bit++;

          if ((bit == 8) || ((s + 4) >= scanline_end)) {
            /* Handle transparent areas of the icon */
            if (mask)
               byte &= bitrev[*m++];

            /* If this is a grayed-out inactive icon, superimpose a 
               checkerboard mask over the data */
            if (inactive_icon_p)
              byte &= 0x55 << row_parity;

            fprintf (stdout, "\\%03o", byte);
            column += 4;
            bit = 0;
            byte = 0;
          }
        }
      scanline += img_header->widthBytes;
      if (mask)
          mask_scanline += mask_header->widthBytes;
    }

  fprintf (stdout, "\",\n");
  column = 0;

  /* Mask data */
  if (mask)
    {
      scanline = mask->bits;
      end = scanline + (mask_header->widthBytes * mask_header->height);
      fprintf (stdout, "\n /* Mask Data */\n (unsigned char*)\n \"");
      column = 2;
      for (;scanline < end; scanline += mask_header->widthBytes)
        {
          unsigned char *scanline_end =
              scanline + ((mask_header->width + 7) / 8);

          for (s = scanline; s < scanline_end; s++)
            {
              if (column > 74)
                {
                  fprintf (stdout, "\"\n \"");
                  column = 2;
                }
              fprintf (stdout, "\\%03o", bitrev[*s]);
              column += 4;
            }
        }

      fprintf (stdout, "\",\n");
      column = 0;
    }
  else
    {
      fprintf (stdout, "\n  0,\n");
      column = 0;
    }


  /* Color icon */
  fprintf (stdout, "\n /* Color icon */\n (unsigned char*)\n \"");
  column = 2;

  scanline = image->bits;
  end = scanline + (img_header->widthBytes * img_header->height);
  for (;scanline < end; scanline += img_header->widthBytes)
    {
      unsigned char *scanline_end = scanline + (img_header->width * 4);

      for (s = scanline; s < scanline_end; s += 4)
        {
          unsigned char r = s[3];
          unsigned char g = s[2];
          unsigned char b = s[1];
          int j;

		  if (!haveSeenTransparentColor &&
			  r == transparent_color[0] &&
			  g == transparent_color[1] &&
			  b == transparent_color[2]) {
			haveSeenTransparentColor = TRUE;

			/* Referenced the transparent color */
			fprintf (stderr, "Warning:  Transparent color, (%3d, %3d, %3d),"
					         " used in a non-transparent way in %s\n",
					 r, g, b, currentIconName);

			if (!haveAddedTransparentColor) {
			  haveAddedTransparentColor = TRUE;
			  fprintf (stderr, "\t adding to the global colormap,"
					   " but this should be fixed.\n");
			  
			  /* Add it to the colormap to let us proceed */
			  global_cmap.map[global_cmap.num_colors].red   = r;
			  global_cmap.map[global_cmap.num_colors].green = g;
			  global_cmap.map[global_cmap.num_colors].blue  = b;
			  
			  global_cmap.num_colors++;
			  if (global_cmap.num_colors > 100) {
				fprintf(stderr, "Global color map exceeded 100 colors in %s\n",
						currentIconName);
				abort();
			  }
			}
		  }

          for (j = 0; j < global_cmap.num_colors; j++) {
			if (r == global_cmap.map[j].red &&
                g == global_cmap.map[j].green &&
                b == global_cmap.map[j].blue)
              {
#ifdef DEBUG
				is_global_cmap_color_used[j] = TRUE;
#endif
                if (column > 74)
                  {
                    fprintf (stdout, "\"\n \"");
                    column = 2;
                  }
                fprintf (stdout, "\\%03o", j);
                column += 4;
                goto DONE;
              }
		  }
		  fprintf (stderr, "Color not found in global colormap:"
				   " (%3d, %3d, %3d) in %s\n",
				   r, g, b, currentIconName);
		  abort();
		DONE:
		  ;
		}
	}
  if (!in_anim) {
    fprintf (stdout, "\"\n};\n\n");
	if (iconIsNotUsedInMozLite) fprintf (stdout, "#endif /* !MOZ_LITE */\n");
    fprintf (stdout, "#endif /* %s */\n\n", macro);
  }

  column = 0;
}


JMC_PUBLIC_API(void)
_IMGCB_GetIconDimensions(IMGCB* img_cb, jint op, void* dpy_cx, int* width,
                         int* height, jint icon_number)
{

}


JMC_PUBLIC_API(void)
_IMGCB_DisplayIcon(IMGCB* img_cb, jint op, void* dpy_cx, jint x, jint y,
                   jint icon_number)
{

}

/*****************************************************************************/
/*                       End of Image Library callbacks                      */
/*****************************************************************************/


/* Image observer callback. */
static void
mkicon_ImageObserver(XP_Observable observable, XP_ObservableMsg message,
                     void *message_data, void *closure)
{
    switch(message) {
    case IL_DIMENSIONS:
        /* Get the total number of (non-unique) colors in the colormap.
           We check for a negative value to confirm that the image library
           has not yet determined the number of unique colors.  If by
           chance, we are unable to get the number of non-unique colors,
           then use the number of unique colors and hope that the unique
           colors are grouped together at the beginning of the colormap. */
        if (src_cmap->num_colors < 0)
            src_cmap_num_colors = -src_cmap->num_colors;
        else
            src_cmap_num_colors = src_cmap->num_colors;

        break;
        
    default:
        break;
    }
}


/* This is essentially a customized version of IL_GetImage, which is the
   Image Library's conventional entry point.  MKICON_GetImage differs from
   IL_GetImage in that it always retrieves the image data from a file, and
   never from the the network or the image cache. */
void
MKICON_GetImage(char *file)
{
  static int counter = 0;
  FILE *fp;
  char *file_data;
  int size;
  struct stat st;
  char *s;
  XP_Bool wrote_header = FALSE;
  il_container *ic;             /* Image container. */
  IL_ImageReq *image_req;       /* The image request (client of container.) */
  IL_GroupContext *img_cx;      /* Image context. */
  IMGCB* img_cb;                /* JMC callback interface to FE. */
  JMCException *exc = NULL;
  IL_RGBBits rgb;
  IL_DisplayData display_data;
  URL_Struct *url;
  NET_StreamClass *stream;
  MWContext cx;

  memset (&cx, 0, sizeof(cx));
  
  /* Create a set of FE callbacks for the Image Library. */
  img_cb = IMGCBFactory_Create(&exc); /* JMC Module */
  if (exc) {
      JMC_DELETE_EXCEPTION(&exc);/* XXXM12N Should really return exception */
      return;
  }

  /* Create an Image Group Context.  IL_NewGroupContext augments the
     reference count for the JMC callback interface.  The opaque argument
     to IL_NewGroupContext is the Front End's display context, which will
     be passed back to all the Image Library's FE callbacks. */
  img_cx = IL_NewGroupContext((void*)&cx, (IMGCBIF *)img_cb);

  /* Attach the IL_GroupContext to the FE's display context. */
  cx.img_cx = img_cx;

  /* Make a container. */
  ic = XP_NEW_ZAP(il_container);
  ic->state = IC_START;
  ic->hash = 0;
  ic->urlhash = 0;
  ic->img_cx = img_cx;
  ic->forced = 0;
  ic->next = 0;
  ic->background_color = NULL;  /* Indicates that a mask should be generated
                                   if the source image is determined to be
                                   transparent. */

  /* The container needs to have a NetContext. */
  ic->net_cx = IL_NewDummyNetContext(&cx, 0);

  /* Allocate the source image header. */
  ic->src_header = XP_NEW_ZAP(NI_PixmapHeader);

  /* Allocate the source image's colorspace.  The fields will be filled in
     by the image decoder. */
  ic->src_header->color_space = XP_NEW_ZAP(IL_ColorSpace);
  IL_AddRefToColorSpace(ic->src_header->color_space);

  /* Hold on to the source image's colormap. */
  src_cmap = &ic->src_header->color_space->cmap;

  /* Allocate the destination image structure. */
  ic->image = XP_NEW_ZAP(IL_Pixmap);

  /* Create the destination image's colorspace.
     XXXM12N We should really be decoding to the source image's colorspace,
     i.e. indexed color for GIF.  However, this requires changes in the Image
     Library which have yet to occur as of 11/18/96.  Hence we continue to use
     the roundabout approach of decoding to a true colorspace and then
     figuring out what the colormap indices are.  This should eventually
     change. */
  rgb.red_bits = 8;
  rgb.green_bits = 8;
  rgb.blue_bits = 8;
#if defined(IS_LITTLE_ENDIAN)
  rgb.red_shift = 24;
  rgb.green_shift = 16;
  rgb.blue_shift = 8;
#elif defined(IS_BIG_ENDIAN)
  rgb.red_shift = 0;
  rgb.green_shift = 8;
  rgb.blue_shift = 16;
#else
  ERROR!  Endianness unknown.
#endif
  ic->image->header.color_space = IL_CreateTrueColorSpace(&rgb, 32);

  /* The container must have a valid client to be serviced. */
  image_req = XP_NEW_ZAP(IL_ImageReq);
  image_req->img_cx = img_cx;
  XP_NewObserverList(image_req, &image_req->obs_list);
  XP_AddObserver(image_req->obs_list, mkicon_ImageObserver, NULL);
  ic->clients = image_req;

  /* Set the display preferences. */
  display_data.color_space = ic->image->header.color_space;
  display_data.progressive_display = PR_FALSE;
  display_data.dither_mode = IL_ClosestColor;
  IL_SetDisplayMode(img_cx, IL_COLOR_SPACE | IL_PROGRESSIVE_DISPLAY |
                    IL_DITHER_MODE, &display_data);

  /* Create a URL structure. */
  url = XP_NEW_ZAP (URL_Struct);
  url->address = strdup("\000\000");
  url->fe_data = ic;

  /* Read in the file data. */
  if (stat (file, &st))
    return;
  size = st.st_size;

  file_data = (char *) malloc (size + 1);
  if ((fp = fopen (file, "r")) == NULL)
	{
	  fprintf(stderr, "Unable to open file: %s\n", file);
	  abort();
	}
  fread (file_data, 1, size, fp);
  fclose (fp);


  /* Start generating code. */
  s = strrchr (file, '.');
  if (s) *s = 0;
  s = strrchr (file, '/');
  if (s)
    s++;
  else
    s = file;

  if (in_anim && strncmp (s, "Anim", 4)) {
    /* once you've started anim frames, don't stop. */
    fprintf (stderr, "Incomplete animation\n");
    abort ();
  }

  if ((!strcmp (s, "AnimSm00") || !strcmp (s, "AnimHuge00")) ||
      ((!strcmp (s, "AnimSm01") || !strcmp (s, "AnimHuge01")) &&
       (!in_anim ||
	anim_frames[in_anim-1] > 1)))
    {
      char *s2;
      if (in_anim)
	{
	  fprintf (stdout, "\"\n }\n};\n\n");
	  fprintf (stdout, "#endif /* %s */\n\n", macro);
	  if (sgi_p)
	    {
	      fprintf (stdout, "\n#endif /* __sgi */\n\n");
	      sgi_p = FALSE;
	    }
	}

      s2 = s - 2;
      while (s2 > file && *s2 != '/')
	s2--;
      s[-1] = 0;
      if (*s2 == '/')
	s2++;

      if (strstr (s2, "sgi"))
	{
	  fprintf (stdout, "#ifdef __sgi\n");
	  sgi_p = TRUE;
	}

      fprintf (stdout, "#ifdef %s\n\n", macro);
      fprintf (stdout, "struct fe_icon_data anim_%s_%s[] = {\n",
	       s2, (!strncmp (s, "AnimHuge", 8) ? "large" : "small"));
      wrote_header = TRUE;
      in_anim++;
    }

  if (in_anim)
    {
      if (strncmp (s, "Anim", 4)) {
		fprintf (stderr, "Expected animation frame\n");
		abort ();
	  }
      if (!wrote_header)
	fprintf (stdout, "\"\n },\n\n");
      fprintf (stdout, "  /* %s */\n  { ", s);
      anim_frames[in_anim-1]++;
    }
  else
    {
      char *s2 = s;
      while (*s2)
	{
	  if (*s2 == '.') *s2 = '_';
	  s2++;
	}

      fprintf (stdout, "#ifdef %s\n", macro);
	  if (iconIsNotUsedInMozLite) fprintf (stdout, "#ifndef MOZ_LITE\n");
      fprintf (stdout, "\nstruct fe_icon_data %s = {\n", s);
    }


  column = 0;
  did_size = FALSE;

  url->address[0] = counter++;

  /* Create a stream and decode the image. */
  stream = IL_NewStream (FO_PRESENT,
			 (strstr (file, ".gif") ? (void *) IL_GIF :
			  strstr (file, ".jpg") ? (void *) IL_JPEG :
			  strstr (file, ".jpeg") ? (void *) IL_JPEG :
			  strstr (file, ".xbm") ? (void *) IL_XBM :
			  (void *) IL_GIF),
			 url, &cx);
  stream->put_block (stream, file_data, size);
  stream->complete (stream);

  inactive_icon_p = (strstr(file, ".i.gif") != NULL);

 /* keep track of this for debugging purposes */
  currentIconName = s;
  haveSeenTransparentColor = FALSE;

  /* Write out the decoded image data. */
  IMGCB_DisplayPixmap(img_cx->img_cb, img_cx->dpy_cx, ic->image, ic->mask,
                      0, 0, 0, 0, ic->image->header.width,
                      ic->image->header.height);

  /* Clean up. */
  free (file_data);
  IMGCB_DestroyPixmap(img_cx->img_cb, img_cx->dpy_cx, ic->image);
  if (ic->mask)
      IMGCB_DestroyPixmap(img_cx->img_cb, img_cx->dpy_cx, ic->mask);
  IL_DestroyGroupContext(img_cx);
  IL_ReleaseColorSpace(ic->src_header->color_space);
  IL_ReleaseColorSpace(ic->image->header.color_space);
  XP_FREE(ic->src_header);
  XP_FREE(ic->image);
  XP_FREE(ic);
  XP_RemoveObserver(image_req->obs_list, mkicon_ImageObserver, NULL);
  XP_DisposeObserverList(image_req->obs_list);
  XP_FREE(image_req);
  XP_FREE(url);
  XP_FREE(stream);
  src_cmap = NULL;
  src_cmap_num_colors = 0;

  /* Move on to the next image icon. */
  total_images++;
}

static void
process_filename(char* filename)
{
    char* p;

	inactive_icon_p = (strstr(filename, ".i.gif") != NULL);

	if (p = strstr(filename, " LITE")) {
	  *p = '\0';
	  iconIsNotUsedInMozLite = FALSE;
	} else {
	  iconIsNotUsedInMozLite = TRUE;
	}

    p = strrchr(filename, '/') ? (strrchr(filename, '/') + 1) : filename;

    if ( *p == '_' ) {
        strcpy(macro, p+1);
        *strchr(macro, '.') = '\0';
    } else {
        strcpy(macro, "XFE");
    }

	MKICON_GetImage(filename);
}

static void
process_stdin(void)
{
	char buf[1024];
	
	while (gets(buf) != NULL) {
		process_filename(buf);
	}
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  int i;

  init_reverse_bits();

  /* Create a global colormap which will be the union of the individual icon
     colormaps. */
  global_cmap.map = (NI_RGB*)calloc(256, sizeof(NI_RGB));
  global_cmap.num_colors = 0;
#ifdef DEBUG
  XP_BZERO(is_global_cmap_color_used, sizeof(XP_Bool) * 256);
#endif

  fprintf (stdout,
	   "/* -*- Mode: Fundamental -*- */\n\n#include \"icondata.h\"\n\n");

  fputs(header_comment, stdout);

  if ( !strcmp(argv[1], "-no-xfe-define") ) {
      argv++, argc--;
  } else {
      fprintf (stdout, "\n#define XFE\n");
  }

  /* Generate output for each icon in the argument list. */
  for (i = 1; i < argc; i++)
    {
      char *filename = argv[i];
	  if (strcmp(filename, "-") == 0) {
		  process_stdin();
	  } else {
		  process_filename(filename);
	  }
    }

  if (in_anim)
    {
      fprintf (stdout, "\"\n }\n};\n\n");
      fprintf (stdout, "#endif /* %s */\n\n", macro);
      if (sgi_p)
	{
	  fprintf (stdout, "\n#endif /* __sgi */\n\n");
	  sgi_p = FALSE;
	}
    }

  fprintf (stdout, "unsigned int fe_n_icon_colors = %d;\n",
           global_cmap.num_colors);

  if (in_anim)
    {
      fprintf (stdout, "unsigned int fe_anim_frames[%d] = { ", MAX_ANIMS);
      i = 0;
      while (anim_frames[i])
	{
	  fprintf (stdout, "%d%s", anim_frames[i],
		   anim_frames[i+1] ? ", " : "");
	  i++;
	}
      fprintf (stdout, " };\n\n");
    }

  /* Output the global colormap. */
  fprintf (stdout, "unsigned short fe_icon_colors[256][3] = {\n");
  for (i = 0; i < global_cmap.num_colors; i++)
	{
#ifdef DEBUG
	  if (global_cmap.map[i].red   == transparent_color[0] &&
		  global_cmap.map[i].green == transparent_color[1] &&
		  global_cmap.map[i].blue  == transparent_color[2])
		{
		  fprintf (stderr, "Warning: Transparent color, %2d:( %3d, %3d, %3d ),"
				   " used in a non-transparent way.\n"
				   "\tCheck the log for the offending icon(s).\n",
				   i, global_cmap.map[i].red,
				   global_cmap.map[i].green,
				   global_cmap.map[i].blue);
		} 
	  else if (!is_global_cmap_color_used[i]) 
		{
		  fprintf (stderr, "Warning: global color map entry, "
				   "%2d:( %3d, %3d, %3d ), never referenced\n",
				   i, global_cmap.map[i].red,
				   global_cmap.map[i].green,
				   global_cmap.map[i].blue);
		}
#endif /* DEBUG */

	  fprintf (stdout, " { 0x%02x%02x, 0x%02x%02x, 0x%02x%02x }%s"
			   "    /* %2d:  %3d, %3d, %3d  */\n",
	     global_cmap.map[i].red,   global_cmap.map[i].red,
	     global_cmap.map[i].green, global_cmap.map[i].green,
	     global_cmap.map[i].blue,  global_cmap.map[i].blue,
		 (i == global_cmap.num_colors-1) ? " " : ",",
		 i, global_cmap.map[i].red,
	     global_cmap.map[i].green,
		 global_cmap.map[i].blue);
	}
  if (global_cmap.num_colors == 0 ) {
      fprintf(stdout, "0\n");
  }

  fprintf (stdout, "};\n");

  /* Clean up. */
  free(global_cmap.map);
  
  return 0;
}


#ifndef M12N
/********************* XXXM12N Fix me! ***************************************/
int
main (argc, argv)
     int argc;
     char **argv;
{
    XP_TRACE(("M12N: mk_anim does not yet work with new Image Library"));
}
/********************** XXXM12N **********************************************/
#endif /* M12N */
