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
/* 
   colors.c --- X front-end stuff dealing with color allocation/sharing
 */


#include "mozilla.h"
#include "xfe.h"

#include "libimg.h"             /* Image Library public API. */
#include "il_util.h"            /* Colormap/Colorspace API. */

#include "xp_qsort.h"

/* for XP_GetString() */
#include <xpgetstr.h>
extern int XFE_VISUAL_GRAY_DIRECTCOLOR;
extern int XFE_VISUAL_GRAY;
extern int XFE_VISUAL_DIRECTCOLOR;
extern int XFE_VISUAL_NORMAL;
extern int  XFE_WILL_BE_DISPLAYED_IN_MONOCHROME;
extern int  XFE_WILL_LOOK_BAD;

/* Convert from an 8-bit imagelib color value to 16-bit X color.

   There is a weirdness here to account for some peculiar servers that
   label their colors incorrectly, e.g. AIX and at least one Linux
   server.  The X spec says that the 16-bit color components range
   from #0000 to #FFFF and that servers are supposed to scale
   intermediate values to this range.  These weird servers, however,
   which I will refer to as "broken servers" for short, label their
   maximum color as #FF00 and lower intensity colors as #FE00, #FD00,
   etc.  This means, for example, that if we ask for color #FDFD, some
   servers will allocate #FDFD as the nearest color that the server
   hardware can realize, but the broken servers will allocate #FE00 as
   the closest color.
   
   To handle this case, we interrogate the server during
   initialization to find out how it numbers its colors and construct
   a mask that indicates whether or not to duplicate the 8-bit color
   in both the upper and lower byte of the 16-bit X color.
 */
#define C8to16(c)           ((((c) << 8) | (c)) & fe_colormask)

/* Convert from 16-bit X color value to 8-bit LO color */
#define C16to8(c)           ((uint16)(c) >> 8)

#define STATIC_VISUAL_P(v)  (((v) != PseudoColor)  && ((v) != GrayScale))
#define GRAYSCALE_WORKS

/* This encapsulation of the server's hardware colormap keeps a
   locally cached copy of the colormap data.  This cache is not kept
   perfectly in sync with the server (unless this is a private
   colormap) because other clients may allocate colors.  However, when
   a cached copy of the server colormap is requested, the use of a
   timer update ensures that it is never more than two seconds out of
   date.  (Of course, cells that *we've* allocated are kept up-to-date
   in the cache).  We also keep an allocation map indicating how the
   individual cells in the server's colormap have been allocated
   (read-write/read-only) and when they are to be freed.  */
struct fe_colormap
{
  Display *dpy;
  Visual *visual;
  XVisualInfo *visual_info;
  int num_cells;                /* Number of cells in the visual */
  Colormap cmap;                /* Handle for X Server colormap */
  XColor *cache;                /* Local cache of server colormap */
  time_t cache_update_time;     /* Last time cache was updated */
  uint8 *allocation;            /* What the color cells are being used for */
  uint16 *transient_ref_count;  /* Reference count: # of CELL_TRANSIENT
                                   color cache entries using this cell */
  Pixel *mapping;               /* Image to server pixel map */
  int mapping_size;             /* Allowed input range of mapping */
  Boolean private_p;            /* We don't share cmap with other X clients */
  int contexts;                 /* Reference count for MWContexts */
  int persistent_colors;        /* CELL_PERMANENT or (CELL_PERMANENT|CELL_IMAGE) */
  Boolean writeable_cells_allocated; /* Possible only if private_p is set */
  IL_ColorSpace *color_space;   /* Image library color conversion state */
};

/* Types of color allocations (flags are exclusive) */
#define  CELL_AVAIL      0x01  /* We don't use cell (other clients might) */
#define  CELL_MARKED     0x02  /* Temporary value */
#define  CELL_SHARED     0x04  /* Read-only cell that we share */
#define  CELL_PRIVATE    0x08  /* Read/write cell that we own */

#define  CELL_ALLOCATION  (CELL_AVAIL|CELL_MARKED|CELL_SHARED|CELL_PRIVATE)

/* Color lifetime flags (these flags are *not* exclusive):
 
   Permanent colors remain allocated for the duration of the
   application.  Image colors are also allocated for the life of the
   application, but they use writeable cells that are mutated when a
   new set of images is displayed.  Transient colors are used for
   text, backgrounds, etc. and are deallocated when the next page
   begins loading.  */
#define  CELL_PERMANENT  0x40  /* widget/icon colors */
#define  CELL_TRANSIENT  0x20  /* Per-page colors, e.g. text colors */
#define  CELL_IMAGE      0x10  /* May last longer than display of page */

#define  CELL_LIFETIME   (CELL_PERMANENT | CELL_TRANSIENT | CELL_IMAGE)

static int fe_colormask = 0;

static int fe_clear_colormap (fe_colormap *colormap, Boolean grab_p);
  
#ifdef DEBUG
void
fe_print_colormap_allocation(fe_colormap *colormap)
{
  int i;
  char c;

  uint8 *allocation = colormap->allocation;
  char buffer[300];

  char *b = buffer;

  /*
    Map printing key:

    For shared, read-only cells:

      char  TRANSIENT     PERMANENT     IMAGE   
      ---------------------------------------
      '.'      0              0           0
      'i'      0              0           1
      'p'      0              1           0
      'I'      0              1           1
      't'      1              0           0
      '?'      1              0           1
      'T'      1              1           0
      'A'      1              1           1

    Writeable cells are marked with '*' and must be image cells.
    Unallocated cells are marked '.'
 
  */

  for (i = 0; i < colormap->num_cells; i++)
    {
      if (allocation[i] & CELL_PRIVATE)
        {
          assert (allocation[i] == (CELL_PRIVATE|CELL_IMAGE));
          c = '*';
        }
      else if (allocation[i] & CELL_TRANSIENT)
        if (allocation[i] & CELL_PERMANENT)
          if (allocation[i] & CELL_IMAGE)
            c = 'A';
          else
            c = 'T';
        else
          c = 't';
      else if (allocation[i] & CELL_PERMANENT)
        if (allocation [i] & CELL_IMAGE)
          c = 'I';
        else
          c = 'p';
      else if (allocation[i] & CELL_IMAGE)
        c = 'i';
      else if (allocation[i] == CELL_AVAIL)
        c = '.';
      else
        c = '?';

      *b++ = c;
        
      if ((i & 15) == 15)
        *b++ = '\n';
    }
  
  *b++ = 0;

  printf("%s", buffer);
}
#endif /* DEBUG */

static void
fe_delete_colormap(fe_colormap *colormap)
{
  if (colormap->visual_info)
    XFree ((char *) colormap->visual_info);
  if (colormap->cache)
    free(colormap->cache);
  if (colormap->allocation)
    free(colormap->allocation);
  if (colormap->transient_ref_count)
    free(colormap->transient_ref_count);
  if (colormap->mapping)
    free(colormap->mapping);
  free(colormap);
}

fe_colormap *
fe_NewColormap(Screen *screen, Visual *visual,
               Colormap cmap, Boolean private_p)
{
  uint8 *allocation;
  uint16 *transient_ref_count;
  int i, num_cells;
  int out_count;
  XColor *cache;
  XVisualInfo vi_in, *vi_out;
  Display *dpy = DisplayOfScreen(screen);

  fe_colormap *colormap = calloc(sizeof(fe_colormap), 1);
  if (!colormap)
    return NULL;

  /* See if this server has screwed up color matching.
     (See the comment near the C8to16() macro.) */
  if (!fe_colormask)
    {
      XColor white_color;
      white_color.red = white_color.green = white_color.blue = 0xffff;
      if (XAllocColor(dpy, cmap, &white_color))
        fe_colormask = white_color.red; /* either 0xffff or 0xff00 */

      /* The XLookupColor should alway succeed, but be paranoid. */
      if ((fe_colormask != 0xff00) && (fe_colormask != 0xffff))
        fe_colormask = 0xffff;
    }
      
  colormap->dpy = dpy;
  colormap->visual = visual;

  /* Isn't it swell that we have to go through this stuff just to
     do `visual->class' portably?? */
  vi_in.screen = fe_ScreenNumber (screen);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  
  colormap->visual_info = vi_out;

  colormap->cmap = cmap;
  if (vi_out->class == TrueColor)
      return colormap;
  
  colormap->num_cells = num_cells = fe_VisualCells (dpy, visual);
  if (vi_out->class == PseudoColor)
    colormap->private_p = private_p;
  
  allocation = calloc(sizeof(uint8), num_cells);
  colormap->allocation = allocation;
  if (!allocation)
    {
      fe_delete_colormap(colormap);
      return NULL;
    }
  for (i = 0; i < num_cells; i++)
    allocation[i] = CELL_AVAIL;

  transient_ref_count = (uint16*)calloc(sizeof(uint16), num_cells);
  colormap->transient_ref_count = transient_ref_count;
  if (!transient_ref_count)
    {
      fe_delete_colormap(colormap);
      return NULL;
    }
  
  /* The colormap cache itself gets allocated when it is requested. */
  colormap->cache_update_time = 0;
      
  cache = (XColor *) calloc (sizeof (XColor), num_cells);
  if (!cache)
    {
      fe_delete_colormap(colormap);
      return NULL;
    }
  colormap->cache = cache;
  for (i = 0; i < num_cells; i++)
    cache[i].pixel = i;

  /* Image colors are treated the same as permanent icon/widget colors
     for shared colormaps (once set, they are never changed).  For
     private colormaps, they may be allocated as writeable cells and can
     change color on-the-fly. */
  if (private_p)
    colormap->persistent_colors = CELL_PERMANENT;
  else
    colormap->persistent_colors = (CELL_IMAGE | CELL_PERMANENT);

  colormap->mapping = (Pixel *) calloc (256, sizeof (Pixel));
  /* Identity mapping from image to server colormap indices */
  /* XXX - this is so uncool. Why not get rid of mapping altogether ? */
  for (i = 0; i < 256; i++)
      colormap->mapping[i] = i;

#if 0                           /* For debug */
  fe_clear_colormap(colormap, False);
#endif

  return colormap;
}

Colormap
XFE_GetDefaultColormap()
{
    /* Used by AWT */
    return fe_cmap(fe_all_MWContexts->context);
}

int
XFE_GetMaxColors()
{
    /* Used by AWT */
    if (fe_globalData.max_image_colors > 0)
        return fe_globalData.max_image_colors;
    return 256;
}

Colormap
fe_cmap(MWContext *context)
{
  return CONTEXT_DATA (context)->colormap->cmap;
}

Colormap
fe_getColormap(fe_colormap *colormap)
{
  return colormap->cmap;
}

Pixel *fe_ColormapMapping(MWContext *context)
{
  return CONTEXT_DATA (context)->colormap->mapping;
}

static void
fe_free_colormap_cells(MWContext *context, int lifetime)
{
  int fp, new_fp, i, j;
  fe_colormap *colormap = CONTEXT_DATA (context)->colormap;
  Display *dpy = colormap->dpy; 
  Colormap cmap = colormap->cmap;
  
  if ((fp = CONTEXT_DATA (context)->color_fp))
    {
      unsigned long *cells_to_free = (unsigned long *)
	malloc (sizeof (unsigned long) * (fp + 1));
      XColor *color_data = CONTEXT_DATA (context)->color_data;
      new_fp = 0;
      
      for (i = 0, j = 0; i < fp; i++) {
        if (color_data[i].flags & lifetime)
          {
            unsigned long pixel = color_data[i].pixel;
            cells_to_free [j++] = pixel;
            if (lifetime == CELL_TRANSIENT)
              {
                assert(colormap->transient_ref_count[pixel]);
                colormap->transient_ref_count[pixel]--;
              }
          }
        else
          color_data[new_fp++] = color_data[i];
      }

      CONTEXT_DATA (context)->color_fp = new_fp;
      if (j)
        XFreeColors (dpy, cmap, cells_to_free, j, 0);
      free (cells_to_free);
    }
}

static void
fe_free_context_image_cells(MWContext *context)
{
  int i;
  fe_colormap *colormap = CONTEXT_DATA (context)->colormap;
  int num_cells = colormap->num_cells;
  uint8 *allocation = colormap->allocation;

 if (STATIC_VISUAL_P(colormap->visual_info->class))
    return;
  
  fe_free_colormap_cells(context, CELL_IMAGE);
  
  for (i = 0; i < num_cells; i++)
    if (allocation[i] & CELL_IMAGE)
      {
        allocation[i] &= ~CELL_IMAGE;
        if (!(allocation[i] & CELL_ALLOCATION))
          allocation[i] = CELL_AVAIL;
      }
}

static void
fe_free_all_image_cells(fe_colormap *colormap)
{ 
  struct fe_MWContext_cons *cons;
  for (cons = fe_all_MWContexts ; cons ; cons = cons->next)
    {
      if (CONTEXT_DATA (cons->context)->colormap == colormap)
        fe_free_context_image_cells(cons->context);
    }
}


/* #define PRINT_COLOR_ALLOC_MAP */

/* Called at the end of a page, this function is used to free all the
   temporary colors used for fonts, links, backgrounds, etc.
 */
void
fe_FreeTransientColors(MWContext *context)
{
  unsigned long i;
  int num_cells;
  int pixel;
  fe_colormap *colormap;
  fe_ContextData *context_data;

  XP_ASSERT(context);
  if (!context) return;

  context_data = CONTEXT_DATA(context);
  XP_ASSERT(context_data);
  if (!context_data) return;
  
  colormap = context_data->colormap;
  XP_ASSERT(colormap);
  if (!colormap) return;
  
  /* colormap is shared, by the time this function is called
     visual_info by be NULL because it was deleted with
     earlier deletion of a context that reference to it.

     Therefore, it's better to check each pointer before
     referencing it. I have found in Frame destrcution,
     fe_delete_colormap was called. It will free visual_info
     with one context. While other frames are hanging on
     to this visual info */

  if (  colormap && colormap->visual_info &&
	STATIC_VISUAL_P(colormap->visual_info->class))
    return;

#ifdef PRINT_COLOR_ALLOC_MAP  
  printf("Before:\n");
  fe_print_colormap_allocation(colormap);
#endif
  
  num_cells = colormap->num_cells;

  /* Compress the color_data cache to remove transient color allocations. */
  fe_free_colormap_cells(context, CELL_TRANSIENT);

  /* Now, clear the CELL_TRANSIENT flag bits in the allocation map. */
  for (pixel = 0; pixel < num_cells; pixel++)
    {
      if (!colormap->transient_ref_count[pixel])
        {
          uint8 *allocation = colormap->allocation;
          allocation[pixel] &= ~CELL_TRANSIENT;
          if (!(allocation[pixel] & CELL_LIFETIME))
              allocation[pixel] = CELL_AVAIL;
        }
    }

#ifdef PRINT_COLOR_ALLOC_MAP  
  printf("\nAfter:\n");
  fe_print_colormap_allocation(colormap);
#endif

#ifdef DEBUG
  /* There shouldn't be any transient colors left allocated when
     the calling context is the only user of the colormap. */
  if (colormap->contexts == 1)
    {
      for (i = 0; i < num_cells; i++)
        assert((colormap->allocation[i] & CELL_TRANSIENT) == 0);
    }
#endif /* DEBUG */
}

void
fe_DisposeColormap(MWContext *context)
{
  fe_colormap *colormap = CONTEXT_DATA (context)->colormap;

  fe_FreeTransientColors(context);
  
  /* Decrement reference count */
  if (--colormap->contexts)
    return;

  if (CONTEXT_DATA (context)->colormap == fe_globalData.common_colormap)
    return;

  fe_DisplayFactoryColormapGoingAway(colormap);

  XFreeColormap(colormap->dpy, colormap->cmap);
  fe_delete_colormap(colormap);
}


/* This allocates as many writeable colormap cells as possible.
   The pixels allocated are returned in allocated and allocated_count.
   This also returns a list of those pixels that *couldn't* be allocated
   in unallocated and unallocated_count.  These are the pixels which are
   already allocated.

   The caller must free the pixels in `allocated'.

   As a side-effect, the allocable pixels will be set to black.
 */
static void
fe_allocate_all_cells (fe_colormap *colormap,
		       unsigned long **allocated, int *allocated_count,
		       unsigned long **unallocated, int *unallocated_count)
{
  int i;
  Display *dpy = colormap->dpy;
  XColor *blacks = 0;
  Colormap cmap = colormap->cmap;
  unsigned long *pixels_tail;
  int num_cells = colormap->num_cells;
  int requested_pixels = num_cells;
  char *allocation_map = 0;

  allocation_map = (char *) calloc (sizeof (char), num_cells);
  *allocated = (unsigned long *) malloc (sizeof (unsigned long) * num_cells);
  *unallocated = (unsigned long *) malloc (sizeof (unsigned long) * num_cells);
  *allocated_count = 0;
  *unallocated_count = 0;

  pixels_tail = *allocated;

  while (requested_pixels > 0)
    {
      if (XAllocColorCells (dpy, cmap, False, 0, 0,
			    pixels_tail, requested_pixels))
	{
#if 1
	  /* we got them all.  Set them to black.
	     This isn't actually necessary, but it helps debugging,
	     and I really like the visual effect on xcmap :-) */
	  if (! blacks)
	    {
	      blacks = calloc (sizeof (XColor), requested_pixels);
	      for (i = 0; i < requested_pixels; i++)
		blacks[i].flags = DoRed|DoGreen|DoBlue;
	    }
	  for (i = 0; i < requested_pixels; i++)
            blacks[i].pixel = pixels_tail[i];
	  XStoreColors (dpy, cmap, blacks, requested_pixels);
#endif

	  for (i = 0; i < requested_pixels; i++)
            {
              allocation_map [pixels_tail [i]] = 1;
              colormap->allocation[pixels_tail[i]] = CELL_PRIVATE;
            }

	  *allocated_count += requested_pixels;
	  pixels_tail += requested_pixels;
	}
      else
	{
	  /* We didn't get all/any of the pixels we asked for.  This time, ask
	     for half as many.  (If we do get all that we ask for, we ask for
	     the same number again next time, so we only do O(log(n)) server
	     roundtrips.)
	   */
	  requested_pixels = requested_pixels / 2;
	}
    }

  /* Now we have allocated as many pixels as we could.
     Now build up a list of the ones we didn't allocate.
   */
  pixels_tail = *unallocated;
  for (i = 0; i < num_cells; i++)
    {
      if (allocation_map [i])  /* This one was free - ignore it. */
	continue;
      *pixels_tail++ = i;
    }
  *unallocated_count = pixels_tail - *unallocated;

  free (allocation_map);
  if (blacks) free (blacks);
}


/* Returns the number of unallocated cells in the map, and as
   a side-effect, sets those cells to black. */
static int
fe_clear_colormap (fe_colormap *colormap,
		   Boolean grab_p)
{
  int i;
  Display *dpy = colormap->dpy;
  Colormap cmap = colormap->cmap;
  unsigned long *allocated, *unallocated;
  int allocated_count, unallocated_count;

  if (grab_p)
    XGrabServer (dpy);

  fe_allocate_all_cells (colormap,
			 &allocated, &allocated_count,
			 &unallocated, &unallocated_count);

  if (allocated_count)
    XFreeColors (dpy, cmap, allocated, allocated_count, 0);

#ifdef ASSUME_CELL_PERMANENT
  for (i = 0; i < allocated_count; i++)
    colormap->allocation[allocated[i]] = CELL_PERMANENT;
#else
  for (i = 0; i < allocated_count; i++)
    colormap->allocation[allocated[i]] = CELL_AVAIL;
#endif
  
  if (grab_p)
    {
      XUngrabServer (dpy);
      XSync (dpy, False);
    }
  free (allocated);
  free (unallocated);
  return allocated_count;
}

#if 0
static void
fe_interrogate_colormap (Screen *screen,
			 Visual *visual,
			 Colormap cmap,
			 IL_IRGB **rgb_ret,
			 int *rgb_count_ret,
			 int *free_count_ret)
{
  Display *dpy = DisplayOfScreen (screen);
  int max, nfree, i, j;
  XColor *xcolors;
  IL_IRGB *ilcolors;
  max = fe_VisualCells (dpy, visual);
  /* #### DANGER! DANGER! DANGER! */
  XGrabServer (dpy);
  nfree = fe_clear_colormap (screen, visual, cmap, False);
  ilcolors = (IL_IRGB *) calloc (sizeof (IL_IRGB), max - nfree);
  xcolors = (XColor *) calloc (sizeof (XColor), max);
  for (i = 0; i < max; i++)
    xcolors[i].pixel = i;
  XQueryColors (dpy, cmap, xcolors, max);
  for (i = 0, j = 1; i < max; i++)
    if (xcolors[i].red || xcolors[i].green || xcolors[i].blue)
      {
	ilcolors[j].red   = xcolors[i].red   >> 8;
	ilcolors[j].green = xcolors[i].green >> 8;
	ilcolors[j].blue  = xcolors[i].blue  >> 8;
	ilcolors[j].index = i;
	ilcolors[j].attr  = IL_ATTR_RDONLY;
	j++;
      }
  XUngrabServer (dpy);
  XSync (dpy, False);
  free (xcolors);
  *rgb_ret = ilcolors;
  *rgb_count_ret = j;
  *free_count_ret = nfree;
}
#endif


/* Remembering the static, unchanging parts of a colormap for future reference.
   The idea here is that there is a set of colors that we allocate once at
   startup (for icons, widget backgrounds and borders, shadows, etc) and never
   free or change; and then there are colors that we allocate for images, which
   might change at some point.  But when we create a new window, we should
   endeavor to share the same static cells to prevent flicker in those items,
   whereas flicker in the images themselves is more acceptable.
 */

/* XXX - do we need these variables anymore ? */
static XColor *memorized_colors = 0;
static int n_memorized_colors = 0;

static void
fe_memorize_colormap (MWContext *context)
{
  int i;
  fe_colormap *colormap = CONTEXT_DATA (context)->colormap;
  Display *dpy = colormap->dpy;
  unsigned long *allocated, *unallocated;
  int allocated_count, unallocated_count;

  if (STATIC_VISUAL_P(colormap->visual_info->class))
    return;

  fe_allocate_all_cells (colormap,
			 &allocated, &allocated_count,
			 &unallocated, &unallocated_count);

  if (allocated_count)
    XFreeColors (dpy, colormap->cmap, allocated, allocated_count, 0);
  for (i = 0; i < allocated_count; i++)
    colormap->allocation[allocated[i]] = CELL_AVAIL;
  free (allocated);

  if (memorized_colors)
    free (memorized_colors);
  n_memorized_colors = unallocated_count;
  memorized_colors = calloc (sizeof (XColor), n_memorized_colors);

#ifdef ASSUME_CELL_PERMANENT
  /* For now, we blindly assume that any color that has already been
     allocated in the colormap is one that Motif has previously
     allocated for us as a widget color.  Of course, that assumption
     could be wrong, but we won't find that out until later when we
     try to use this pixel (see fe_AllocClosestColor()). */
/*  for (i = 0; i < n_memorized_colors; i++)
    colormap->allocation[unallocated [i]] = CELL_AVAIL;
*/
#endif
  free (unallocated);
  XQueryColors (dpy, colormap->cmap, memorized_colors, n_memorized_colors);
}

static XColor *
fe_get_server_colormap_cache(fe_colormap *colormap)
{
  int num_cells;
  XColor *cache = colormap->cache;
  Boolean cached;
  time_t now;
  int visual_class = colormap->visual_info->class;

  /* Don't bother pretending about colormaps with static visuals. */
  if (visual_class == TrueColor)
    return NULL;

  cached = (colormap->cache_update_time != 0);

  /* There's no point in loading a StaticColor or StaticGray map more
     than once because it never changes.  Private colormaps are never
     changed by anyone but us so it's pointless to contact the
     server. */
  if ((!cached && (now = time ((time_t) 0))) ||
      ((!colormap->private_p)           &&
       (!STATIC_VISUAL_P(visual_class)) &&
       (now = time ((time_t) 0))        &&
       (colormap->cache_update_time < now)))
    {
      num_cells = colormap->num_cells;
      if (!colormap->private_p)
        XQueryColors (colormap->dpy, colormap->cmap, cache, num_cells);
      colormap->cache_update_time = now;
    }
  return cache;
}




static uint
fe_FindClosestColor (fe_colormap *colormap, XColor *color_in_out,
                     int allocation_flags)
{
  int num_cells = colormap->num_cells;
  XColor color;
  unsigned long distance = ~0;
  int i, found = 0;
  XColor *cache;

  /* Don't interrogate server's colormap about colors that we've allocated */
  cache = colormap->cache;
  if (!colormap->cache_update_time || (allocation_flags & CELL_AVAIL))
    cache = fe_get_server_colormap_cache(colormap);

  /* Find the closest color. */
  for (i = 0; i < num_cells; i++)
    {
      unsigned long d;
      int rd, gd, bd;

      if (!(colormap->allocation[i] & allocation_flags))
        continue;
      
      rd = C16to8(color_in_out->red)   - C16to8(cache[i].red);
      gd = C16to8(color_in_out->green) - C16to8(cache[i].green);
      bd = C16to8(color_in_out->blue)  - C16to8(cache[i].blue);
      if (rd < 0) rd = -rd;
      if (gd < 0) gd = -gd;
      if (bd < 0) bd = -bd;
      d = (rd << 1) + (gd << 2) + bd;
      
      if (d < distance)
	{
	  distance = d;
	  found = i;
          if (distance == 0)
              break;
	}
    }

  if (distance != ~0)
    {
      color = cache[found];
      color.pixel = found;
      *color_in_out = color;
    }

    return distance;
}

/* Like XAllocColor(), but searches the current colormap for the best
   match - this always successfully allocates some color, hopefully one
   close to that which was requested.

   The only slot of the MWContext that is used is the `server_colormap'
   cache slot - the widget is not used, so it's safe to call this early.
 */
void
fe_AllocClosestColor (fe_colormap *colormap,
                      XColor *color_in_out)
{
  Display *dpy = colormap->dpy;
  Colormap cmap = colormap->cmap;
  int num_cells = colormap->num_cells;
  int retry_count = 0;
  int done, i, distance, expected_pixel;
  XColor color = *color_in_out;

RETRY:
  /* First try to find an exact match for a color we already own.  We
     don't have to interrogate the server for that case.  (Note, however,
     that Motif allocates colors for us that we don't know about). */
  distance = fe_FindClosestColor(colormap, &color, CELL_SHARED);

  /* An exact color match ? */
  if (distance == 0)
    {
      expected_pixel = color.pixel;
      /* XXX - fur - we can get eventually get rid of this call to
         XAllocColor() since we already own this color, but we need to
         adjust our accounting */
      if (!XAllocColor (dpy, cmap, &color))
      {
#ifdef ASSUME_CELL_PERMANENT
        /* Oops, if we couldn't allocate that color, it means that the
           colormap cell that we thought was a permanent cell that we
           had allocated ourself because we found it already allocated
           when we initialized the colormap is, in fact, a read/write
           cell that belongs to somebody else. */
          assert (allocation & CELL_PERMANENT);
#else
        /* No - we don't make that assumption.  Maybe later, but not yet. */
        assert(0);
#endif

        /* Mark cell temporarily so that we don't try to allocate it again. */
        colormap->allocation[color.pixel] = CELL_MARKED;

        retry_count++;
	goto RETRY;
      }
    }
  else
    {
      /* OK, *we* haven't previously allocated this exact color before.
         Try to find a close color and share it. */
      do
        {
          distance =
              fe_FindClosestColor(colormap, &color, CELL_SHARED|CELL_AVAIL);
          assert(distance != ~0);
          expected_pixel = color.pixel;
 
          if (!(done = XAllocColor (dpy, cmap, &color)))
            {
              /* We couldn't allocate the cell.  Either

                 a) the colormap has changed since we last
                    copied it into the colormap cache from the server, or
 
                 b) the colormap cell that we thought was a
                    permanent cell that we had allocated ourself
                    because we found it already allocated when we
                    initialized the colormap is, in fact, a read/write
                    cell that belongs to somebody else.  So mark this
                    cell as temporarily unavailable and try again.  */
              assert(colormap->allocation[color.pixel] & CELL_AVAIL);

              colormap->allocation[color.pixel] = CELL_MARKED;
              retry_count++;
            }
        } while (!done && retry_count != num_cells);
    }
  
  assert(retry_count != num_cells);
  assert(expected_pixel == color.pixel);
  
  /* Remove all the "temporarily unavailable" marks in the allocation map. */
  for (i = 0; i < num_cells; i++)
    {
      if (colormap->allocation[i] & CELL_MARKED)
        colormap->allocation[i] = CELL_AVAIL;
    }

#if 0
  fprintf (real_stderr,
 #ifdef OSF1
          "substituted %04X %04X %04X for %04X %04X %04X (%ld)\n",
#else
          "substituted %04X %04X %04X for %04X %04X %04X (%ld)\n",
#endif
	   color.red, color.green, color.blue,
	   color_in_out->red, color_in_out->green, color_in_out->blue,
	   distance);
#endif

  *color_in_out = color;
}

int fe_ColorDepth(fe_colormap * colormap)
/*
 * description:
 *
 * returns:
 *	The number of palette entries expressed in the
 *	same manner as bits per pixel, e.g. if 256 colors
 *	are available the return value is 8.
 */
{
  int result = 0;
  if (colormap)
    {
      double x;
      double y;
      x = (double)(colormap->num_cells);
      y = log(x)/log(2.0);
      result = (int)y;
    }
  return result;
}

Status
fe_AllocColor(fe_colormap *colormap, XColor *color_in_out)
{
  Status status = XAllocColor(colormap->dpy, colormap->cmap, color_in_out);
  
  /* Update local cached copy of server-side color map. */
  if (status && colormap->cache)
    colormap->cache[color_in_out->pixel] = *color_in_out;

  return status;
}


/* Calls XAllocColor, caching and reusing the result (without a
   server roundtrip.)  The pixel allocated is never freed until
   the window is deleted.  The RGB arguments are 8 bit values, not 16.
 */
static Pixel
fe_get_shared_pixel (MWContext *context,
                     int r, int g, int b, /* 8-bit color channels */
                     int lifetime)        /* CELL_TRANSIENT, CELL_PERMANENT,
                                             CELL_IMAGE */
{
  int i, fp;
  unsigned long pixel;
  XColor *cd;

  fe_colormap *colormap = CONTEXT_DATA (context)->colormap;

  /* Get color cache data */
  cd = CONTEXT_DATA (context)->color_data;
  fp = CONTEXT_DATA (context)->color_fp;

  /* Create color cache if it doesn't exist. */
  if (!CONTEXT_DATA (context)->color_size)
    {
      CONTEXT_DATA (context)->color_size = 50;
      CONTEXT_DATA (context)->color_data = cd =
	calloc (sizeof (XColor), CONTEXT_DATA (context)->color_size);
    }

  /* Mask, in case someone gave us 16-bit color components */
  r &= 0xff;
  g &= 0xff;
  b &= 0xff;

  /* X's pixel components are 16 bits; LO uses 8. */
  r = C8to16(r);
  g = C8to16(g);
  b = C8to16(b);

  /* Search through the cache of colors we've previously matched */
  if (STATIC_VISUAL_P(colormap->visual_info->class))
    {
      for (i = 0; i < fp; i++) {
        if (cd [i].red == r && cd [i].green == g && cd [i].blue == b)
            return cd [i].pixel;
      }
    }
  else
    {
      for (i = 0; i < fp; i++) {
        unsigned long pixel = cd [i].pixel;
        uint8 *allocation = &colormap->allocation[pixel];
        
        /* Color cell lifetimes must be of the same class, except that
           permanent colors can always be reused */
        if ((*allocation & (lifetime | colormap->persistent_colors)) &&
            (cd [i].red == r && cd [i].green == g && cd [i].blue == b))
          {
            *allocation |= lifetime;
            return cd [i].pixel;
          }
      }
    }

  /* We didn't find the color in the cache.  Try to allocate it.
     Enlarge cache, if necessary. */
  if (fp >= CONTEXT_DATA (context)->color_size)
    {
      CONTEXT_DATA (context)->color_size += 50;
      CONTEXT_DATA (context)->color_data = cd =
	realloc (cd, sizeof (XColor) * CONTEXT_DATA (context)->color_size);
    }

  cd [fp].flags = (DoRed|DoGreen|DoBlue);
  cd [fp].red   = r;
  cd [fp].green = g;
  cd [fp].blue  = b;

  if (!fe_AllocColor (colormap, &cd [fp]))
    fe_AllocClosestColor (colormap, &cd [fp]);

  pixel = cd [fp].pixel;
  cd [fp].flags |= lifetime;

  /* Record the new allocation in the allocation map. */ 
  if (colormap->allocation)
    {
      colormap->allocation[pixel] &= ~CELL_AVAIL;
      colormap->allocation[pixel] |= (CELL_SHARED|lifetime);
      if (lifetime == CELL_TRANSIENT)
        colormap->transient_ref_count[pixel]++;
    }
  
  /* Either XAllocColor or fe_AllocClosestColor gave us back new rgb values
     (the "close" color.)  (XAllocColor will round to the nearest color the
     hardware supports and fail otherwise; fe_AllocClosestColor will round
     to the nearest color that can actually be allocated.  We store the
     values we actually requested back in there so that the cache works... */

  cd [fp].red   = r;
  cd [fp].green = g;
  cd [fp].blue  = b;

  CONTEXT_DATA (context)->color_fp++;

  return pixel;
}

/* Get a transient color that will be released once we're done displaying
   the current page. */
Pixel
fe_GetPixel (MWContext *context, int r, int g, int b)
{
  return fe_get_shared_pixel (context, r, g, b, CELL_TRANSIENT);
}

/* Get a color that will be allocated for the life of the app */
Pixel
fe_GetPermanentPixel (MWContext *context, int r, int g, int b)
{
  return fe_get_shared_pixel (context, r, g, b, CELL_PERMANENT);
}

Pixel
fe_GetImagePixel (MWContext *context, int r, int g, int b)
{
  return fe_get_shared_pixel (context, r, g, b, CELL_IMAGE);
}

#if 0
/* Check to see if all the colors in a map can be allocated */
static int
fe_unavailable_image_colors (MWContext *context, int num_colors,
                             IL_IRGB *map, int allocation_flags)
{
  XColor color;
  uint distance;

  fe_colormap *colormap = CONTEXT_DATA (context)->colormap;
  int close = 0;
  int colors_remaining = num_colors;
  IL_IRGB *c = map;

  while(colors_remaining--)
  {
      color.red   = c->red;
      color.blue  = c->blue;
      color.green = c->green;
      color.flags = DoRed|DoGreen|DoBlue;
      c++;

      distance = fe_FindClosestColor (colormap, &color, allocation_flags);

      if (distance < 10)
          close++;
  }
  
  return num_colors - close;
}
#endif /* 0 */


void
fe_QueryColor (MWContext *context, XColor *color)
{
  XColor *cd = CONTEXT_DATA (context)->color_data;
  int fp = CONTEXT_DATA (context)->color_fp;
  int i;

  /* First look for it in our cached data. */
  if (cd)
    for (i = 0; i < fp; i++)
      if (cd [i].pixel == color->pixel)
	{
	  *color = cd [i];
	  return;
	}

  /* If it's not there, then make a server round-trip. */
  XQueryColor (XtDisplay (CONTEXT_WIDGET (context)),
	       fe_cmap(context), color);
}

#ifdef GRAYSCALE_WORKS

/* This remaps 8-bit gray values to server pixel values, if we're using
   a GrayScale visual, which is different from StaticGray in that the gray
   values indirect through a colormap. */
Pixel fe_gray_map [256] = { 0, };

static Boolean
init_gray_ramp (MWContext *context, Visual *visual)
{
  int i;
  fe_colormap *colormap = CONTEXT_DATA (context)->colormap;
  Display *dpy = colormap->dpy;
  int depth = fe_VisualDepth (dpy, visual);
  int cells = countof (fe_gray_map);

  if (depth == 1)
    return True;

  if ((1 << depth) < cells)
    cells = (1 << depth);

  for (i = 0; i < cells; i++)
    {
      int v = ((i * (countof (fe_gray_map) / cells))
	       + (countof (fe_gray_map) / (cells * 2)));
      fe_gray_map [i] = fe_GetPermanentPixel(context, v, v, v);
    }
  return True;
}

#endif /* GRAYSCALE_WORKS */


/* We don't distinguish between colors that are "closer" together
   than this.  The appropriate setting is a subjective matter. */
#define IL_CLOSE_COLOR_THRESHOLD  6

/* Distance to closest color in map for a given pixel index */
struct color_distance {
  uint distance;
  int index;
};

/* Sorting predicate for qsort() */
static int
compare_color_distances(const void *a, const void *b)
{
    struct color_distance *a1 = (struct color_distance*)a;
    struct color_distance *b1 = (struct color_distance*)b;

    return (a1->distance < b1->distance) ? 1 :
        ((a1->distance > b1->distance) ? -1 : 0);
}

/* When allocating image colors using read/write cells, make sure that
   some are left over for text, backgrounds, etc.  This is in addition
   to any CELL_PERMANENT colors that are allocated when the program
   starts. */
#define NUM_COLORS_RESERVED_FOR_TRANSIENTS   5

/* Force a given palette, relying on close colors.

   When honor_palette_indices is FALSE, we can arbitrarily assign the
   palette index that we want the image library to use, perhaps to
   minimize palette flashing.  This flag is normally set to FALSE the
   first time an image is to be decoded.  When an image is displayed
   subsequent times, IL_ATTR_HONOR_INDEX is set, and the XFE must
   exactly match the specified index in the IL_IRGB struct.  This
   can't simply be a mapping from the image's indices to the server
   indices, because in this case the image pixmap is already resident
   on the server.  Therefore, the index must be mapped to the same
   physical hardware colormap index on the server.

   Image pixels are handled two different ways.  If the colormap is
   private, image colors are allocated as writeable cells (though they
   can also be read-only CELL_PERMANENT cells).  If the colormap is
   shared, image colors are treated essentially the same as
   CELL_PERMANENT colors.

   This function is called only once per colormap if the colormap is not
   a private one.
*/
static int
fe_SetColorMap (MWContext *context,
                 IL_ColorMap *cmap,
                 int num_requested,
                 XP_Bool honor_indices)
{
  /* The image library is requesting `num_requested', possibly
     non-unique colors.

     Prior to this we have allocated three "kinds" of colors: those
     used for the icons/widgets themselves, and those used for text
     and background colors and those used for images.

     At this point, we will free the cells of the last type, while leaving
     the cells of the other types alone (since they are still in use.)  */

  /* Keep track of distance between requested colors and the semi-permanent
     widget colors. */
  struct color_distance color_sort_array[256];
  IL_RGB sorted_map[256], *map_in, *map_out;
  uint8 sorted_pos[256];
  XColor color;
  int i, j, free_count;

  int exact_matches = 0;
  fe_colormap *colormap = CONTEXT_DATA (context)->colormap;
  int num_cells = colormap->num_cells;

  map_in = map_out = cmap->map;
  colormap->mapping_size = num_requested;
  
  /* Create the array of server indices for the colormap, and initialize it
     to the index order of the IL_ColorMap. */
  if (!cmap->index) {
      cmap->index = (uint8*)XP_CALLOC(256, 1);
      if (!cmap->index)
          return 0;
      for (i = 0; i < 256; i++)
          cmap->index[i] = i;
  }

  if (colormap->private_p) {
    XColor colors[256];
    unsigned long *allocated, *unallocated;
    int allocated_count, unallocated_count;
    uint8 *allocation = colormap->allocation;

    if (!colormap->writeable_cells_allocated) {
      fe_allocate_all_cells (colormap,
                             &allocated, &allocated_count,
                             &unallocated, &unallocated_count);

      /* Give back some colors so we have some left for other purposes */
      if (allocated_count < NUM_COLORS_RESERVED_FOR_TRANSIENTS)
        return 0;
      XFreeColors (colormap->dpy, colormap->cmap,
                   allocated, NUM_COLORS_RESERVED_FOR_TRANSIENTS, 0);
      
      for (i = 0; i < allocated_count; i++)
        if (i < NUM_COLORS_RESERVED_FOR_TRANSIENTS)
          colormap->allocation[allocated[i]] = CELL_AVAIL;
        else
          colormap->allocation[allocated[i]] = (CELL_IMAGE | CELL_PRIVATE);
      
      free (unallocated);
      free (allocated);
      colormap->writeable_cells_allocated = True;
    }

    j = 0;
    for (i = 0; i < num_requested; i++)
      {
        uint distance;
        XColor tmpcolor1, tmpcolor2, tmpcolor3, outcolor;
        int red, green, blue;
        int index;

#ifndef M12N                    /* XXXM12N Get rid of this? */
        int index   = map_in[i].index;

        /* "Transparent" colors get mapped to the background */
        if (map_in[i].attr & IL_ATTR_TRANSPARENT)
          {
            map_out[i].index = CONTEXT_DATA (context)->bg_pixel;
            continue;
          }
#endif /* M12N */

        /* X's pixel components are 16 bits; LO uses 8. */
        red   = C8to16(map_in[i].red);
        green = C8to16(map_in[i].green);
        blue  = C8to16(map_in[i].blue);

        color.red   = red;
        color.green = green;
        color.blue  = blue;
        color.flags = DoRed|DoGreen|DoBlue;

#ifndef M12N                    /* XXXM12N Fix me. */
        /* Can we assign an arbitrary palette index to this color ? */
        if (!(map_in[i].attr & IL_ATTR_HONOR_INDEX))
#else
            /* XXXM12N The honor index attribute was used when restoring
               the default colormap after installing a custom colormap.
               We need to fix custom colormaps once we get the rest of M12N
               working. */
        if (!honor_indices)    
#endif /* M12N */
          {
            /* Make a copy to avoid fe_FindClosestColor() side-effects */
            tmpcolor1 = color;

            distance =
              fe_FindClosestColor (colormap, &tmpcolor1, CELL_PERMANENT);
            assert (distance != ~0);
            index = tmpcolor1.pixel;

            /* If we can get a very close shared color, take that instead of a 
               read/write cell */
            if (distance <= IL_CLOSE_COLOR_THRESHOLD)
              {
#ifdef DEBUG
                Status status;
                tmpcolor1.flags = DoRed|DoGreen|DoBlue;
                /* We shouldn't need to allocate a color that we already own. */
                status = fe_AllocColor(colormap, &tmpcolor1);
                assert (status);

                /* Make sure we got the one we asked for */
                assert (tmpcolor1.pixel == index);
#endif
                if (distance == 0)
                  exact_matches++;
                outcolor = tmpcolor1;
              }
            else
              {
                /* Make a copy to avoid fe_FindClosestColor() side-effects */
                tmpcolor2 = color;

                /* See if we've already allocated a private close color. */
                distance =
                  fe_FindClosestColor (colormap, &tmpcolor2, CELL_MARKED);

                index = tmpcolor2.pixel;
                outcolor = tmpcolor2;

                if (distance > IL_CLOSE_COLOR_THRESHOLD)
                  {
                    /* Try to minimize color flashing by picking a cell
                       that's close in color to the desired one.  Yes,
                       yes, I know.  A lost cause. */
                    tmpcolor3 = color;
                    distance =
                      fe_FindClosestColor (colormap, &tmpcolor3, CELL_PRIVATE);

                    /* Any more writeable, private colormap cells left ? */
                    if (distance != ~0)
                      {
                        index = tmpcolor3.pixel;
                
                        /* Temporarily mark the cell */
                        allocation[index] = CELL_MARKED;
                        colors[j] = color;
                        colors[j].pixel = index;
                        colors[j].flags = DoRed|DoGreen|DoBlue;
                        outcolor = colors[j];
                        colormap->cache[index] = outcolor;
                        j++;
                        exact_matches++;
                      }
                  }
                else if (distance == 0)
                  exact_matches++;
              }

            /* Reflect the actual colors and palette indices to our caller. */
            cmap->index[i] = index;
            map_out[i].red   = C16to8(outcolor.red);
            map_out[i].green = C16to8(outcolor.green);
            map_out[i].blue  = C16to8(outcolor.blue);
          }
        else                    /* IL_ATTR_HONOR_INDEX */
          {
            index = cmap->index[i];

            /* Is this a read-only cell ? */
            if (allocation[index] & CELL_SHARED)
              {
#ifdef DEBUG
                /* We shouldn't need to allocate a color that we already own. */
                Status status = fe_AllocColor(colormap, &color);
                assert (status);

                /* Make sure we got the one we asked for */
                assert (color.pixel == index);
                assert (allocation[index] & CELL_PERMANENT);
#endif
              }
            else 
              /* Nope, it must be a writeable cell. */
              {
                assert(allocation[index] & CELL_PRIVATE);
                colors[j] = color;
                colors[j].pixel = index;
                colors[j].flags = DoRed|DoGreen|DoBlue;
                j++;
              }

            exact_matches++;
            
            /* Reflect the actual colors to our caller. */
            map_out[i].red   = C16to8(color.red);
            map_out[i].green = C16to8(color.green);
            map_out[i].blue  = C16to8(color.blue);
          }
      }

    if (j)
      XStoreColors (colormap->dpy, colormap->cmap, colors, j);

    /* Remove all the marks in the allocation map. */
    for (i = 0; i < num_cells; i++)
      {
        if (colormap->allocation[i] & CELL_MARKED)
          colormap->allocation[i] = (CELL_PRIVATE | CELL_IMAGE);
      }
  
    return exact_matches;
  }

  /* Colormap isn't private.  This is our one-and-only initialization call . */

  /* First free all of the image cells, that is, all the cells
     that were allocated by fe_GetPixel() for images.  Compress
     the color_data cache to remove those colors. */

  fe_free_all_image_cells(colormap);
  free_count = fe_clear_colormap(colormap, False);

  /* Sort the requested colors in terms of decreasing distance from any
     existing colors in the palette.  That way, if we run out of
     entries to allocate, the remaining colors, which get allocated to
     the closest existing entry, won't lose as badly. Don't bother to do
     this if there are more free color map entries than requested. */

  if (num_requested > free_count)
    {
      for (i = 0; i < num_requested; i++) {
        uint distance;
      
          color.red   = map_in[i].red;
          color.green = map_in[i].green;
          color.blue  = map_in[i].blue;
      
          distance =
            fe_FindClosestColor (colormap, &color, colormap->persistent_colors);
          color_sort_array[i].distance = distance;
          color_sort_array[i].index = i;
     }

     XP_QSORT(color_sort_array, num_requested, sizeof(struct color_distance),
			  compare_color_distances);

     /* Reorder the request colors to correspond to the sorted order */
     for (i = 0; i < num_requested; i++) {
         j = color_sort_array[i].index;
         sorted_map[i] = map_in[j];
         sorted_pos[i] = j;
     }
     map_in = sorted_map;
  }
  else {
      for (i = 0; i < num_requested; i++)
          sorted_pos[i] = i;
  }

  /* Now attempt to allocate some new cells. */
  {
    XColor *server_map = fe_get_server_colormap_cache(colormap);
    IL_RGB *mp;
    int i;
    /* This will always allocate the requested number of pixels, but in
       some cases will allocate closest-match colors instead, due to the
       magic of fe_get_shared_pixel(). */
    if (num_requested > num_cells)
      abort ();

    /* Allocate the colors. */
    for (i = 0, mp = map_in; i < num_requested; i++, mp++) {
      Pixel pixel;
      int actual_red, actual_blue, actual_green;
      j = sorted_pos[i];
      
#ifndef M12N                    /* XXXM12N Get rid of this? */
      if (mp->attr & IL_ATTR_TRANSPARENT)
          pixel = CONTEXT_DATA (context)->bg_pixel;
      else
#endif /* M12N */
        {
          pixel = fe_GetImagePixel (context, mp->red, mp->green, mp->blue);

          actual_red   = C16to8(server_map[pixel].red);
          actual_green = C16to8(server_map[pixel].green);
          actual_blue  = C16to8(server_map[pixel].blue);

          if ((mp->red == actual_red) && (mp->green == actual_green) &&
              (mp->blue == actual_blue))
              exact_matches++;
      
          /* Reflect the actual quantized color allocated to the requestor. */
          map_out[j].red   = actual_red;
          map_out[j].green = actual_green;
          map_out[j].blue  = actual_blue;
        }
      cmap->index[j] = pixel;

      /* Update the image->server translation map */
      /* im [mp->index] = pixel; */
    }
  }

  return exact_matches;
}


/* Given a mask value, returns the index of the first set bit, and the
   number of consecutive bits set. */
static void
FindShift (unsigned long mask, uint8 *shiftp, uint8* bitsp)
{
  uint8 shift = 0;
  uint8 bits = 0;
  while ((mask & 1) == 0) {
    shift++;
    mask >>= 1;
  }
  while ((mask & 1) == 1) {
    bits++;
    mask >>= 1;
  }
  *shiftp = shift;
  *bitsp = bits;
}

#if 0
/* Initialize the colormap for displaying images.
 */
Colormap
fe_CopyDefaultColormap (Screen *screen, MWContext *new_context)
{
  Colormap default_cmap = DefaultColormapOfScreen (screen);
  /* First, notice which cells are allocated in the default map. */
  fe_memorize_colormap (screen, default_cmap);
  /* Then, make a new map with those same cells allocated. */
  return fe_make_new_colormap_1 (screen, new_context);
}
#endif


static IL_ColorMap *
fe_RealizeDefaultColormap(MWContext *context, int max_colors)
{
    IL_ColorMap *cmap = NULL;
    int num_exact_matches;

    /* XXXM12N This function should have code to pick an appropriate color
       cube, as in the old IL_RealizeDefaultColormap.  For now, we assume
       that we can install a color cube with 216 colors. */

	/* Minimun of max_colors is 8, so we can make a reasonable color cube.
	   of size 2x2x2. */
    if(max_colors < 8)
		max_colors = 8;

	/* Maximum of max_colors is 216. (6x6x6 color cube) */
	if(max_colors > 216) 
		max_colors = 216;
	
	cmap = IL_NewCubeColorMap(NULL, 0, max_colors);
	
	/* This may alter colormap entries to the closest available colors.
	   There is no need to honor the colormap indices the first time we
	   install the colormap. */
	num_exact_matches = fe_SetColorMap(context, cmap, max_colors, FALSE);

    return cmap;
}


void 
fe_InitColormap (MWContext *context)
{
  Display *dpy = XtDisplay (CONTEXT_WIDGET (context));
  fe_colormap *colormap = CONTEXT_DATA (context)->colormap;
  Visual *visual = colormap->visual;
  XVisualInfo *vi_out = colormap->visual_info;
  int pixmap_depth;

  static Boolean warned = False;
  Boolean ugly_visual = False;
  Boolean broken_visual = False;

  /* Remember the state of the colormap before the first color is allocated
     for images.  We only need to do this once - for the first window
     created.  Other windows will have exactly the same non-image cells
     allocated, since we allocate all of those in the same way.

     The rest of this comment is hypothetical / not (yet) true:

     When the default visual is being used with a non-default colormap,
     fe_memorize_colormap() will actually be called twice - first, it will
     be called before we create our first widget, to make sure that we have
     duplicated all of the allocated cells in the default map into our private
     map.  Then it will be called a second time after we have initialized our
     first top-level window, to lock down all of the other cells we've
     allocated, so that subsequent windows will share those cells too.

     This means that any cells which are allocated in the default map at the
     time Netscape starts up will also be allocated in Netscape's map(s).
     This might not be such a good thing, as those colors might suck.
     There probably should be a resource to control this behavior.
   */
  if (!colormap->contexts && !colormap->private_p)
    fe_memorize_colormap (context);
  colormap->contexts++;

  /* Sharing this colormap (and thus its matching colorspace) ? */
  if (colormap->contexts > 1)
      context->color_space = colormap->color_space;

#ifndef M12N
  /* This no longer exists. */
  IL_InitContext (context);
#else
  /* Color spaces, i.e. palettes are one-per-window, so they're inherited from
     the parent context. */
  if (!context->color_space && context->is_grid_cell) {
      XP_ASSERT(context->grid_parent);
      context->color_space = context->grid_parent->color_space;
  }

  /* Increment the reference count if the colorspace exists. */
  if (context->color_space)
      IL_AddRefToColorSpace(context->color_space);
#endif /* M12N */

  pixmap_depth = fe_VisualPixmapDepth (dpy, visual);

  if (! fe_globalData.force_mono_p)
    switch (vi_out->class)
      {
      case StaticGray:
	/* All depths of gray or mono dithered visuals are fine. */
	break;

      case GrayScale:
#ifdef GRAYSCALE_WORKS
	/* To make GrayScale visuals work, we need to allocate a gray
	   ramp in the colormap, and remap the intensity values that
	   the image library gives us into that map. */
	if (! init_gray_ramp (context, visual))
	  broken_visual = True;
#else /* !GRAYSCALE_WORKS */
	if (vi_out->depth > 1)
	  broken_visual = True;
#endif /* !GRAYSCALE_WORKS */
	break;

      case TrueColor:
	/* Shallow non-colormapped visuals work but look bad. */
	if (vi_out->depth < 8)
	  ugly_visual = True;
	break;

      case StaticColor:
	/* StaticColor visuals usually contain a color cube, so they look
	   ok so long as they're not too shallow.  Let's hope no StaticGray
	   visuals claim that they're StaticColor visuals. */
	if (vi_out->depth < 8)
	  ugly_visual = True;
	break;

      case DirectColor:
#ifndef DIRECTCOLOR_WORKS
	/* #### DirectColor visuals are like TrueColor, but have three
	   colormaps - one for each component of RGB.  So the RGB
	   values that the image library gives us are not suitable
	   unless we were to specially prepare the maps, or were
	   to remap the values. */
	broken_visual = True;
#endif /* !DIRECTCOLOR_WORKS */
	break;

      case PseudoColor:
	/* Colormapped visuals are supported ONLY in depth 8. */
	if (vi_out->depth != 8)
	  broken_visual = True;
	break;
      }

  if (ugly_visual || broken_visual)
    {
      char buf [2048];
		char *str = 0;

#ifdef GRAYSCALE_WORKS
#ifdef DIRECTCOLOR_WORKS
		str = XP_GetString( XFE_VISUAL_GRAY_DIRECTCOLOR );
#endif
#endif
#ifdef GRAYSCALE_WORKS
		if( 0 == str ) str = XP_GetString( XFE_VISUAL_GRAY );
#endif
#ifdef DIRECTCOLOR_WORKS
		if( 0 == str ) str = XP_GetString( XFE_VISUAL_DIRECTCOLOR );
#endif
		if( 0 == str ) str = XP_GetString( XFE_VISUAL_NORMAL );


      PR_snprintf (buf, sizeof (buf), str, /* #### i18n */ 
	       (unsigned int) vi_out->visualid,
	       (vi_out->depth == 8 ? "n" : ""),
	       vi_out->depth,
	       (vi_out->class == StaticGray  ? "StaticGray" :
		vi_out->class == StaticColor ? "StaticColor" :
		vi_out->class == TrueColor   ? "TrueColor" :
		vi_out->class == GrayScale   ? "GrayScale" :
		vi_out->class == PseudoColor ? "PseudoColor" :
		vi_out->class == DirectColor ? "DirectColor" : "UNKNOWN"),
	       (broken_visual
		? XP_GetString( XFE_WILL_BE_DISPLAYED_IN_MONOCHROME )
		: XP_GetString( XFE_WILL_LOOK_BAD )), fe_progclass);

      FE_Alert (context, buf);
      warned = True;
    }

  /* Interrogate X server and get definitions of the existing color map.
   */
  if (vi_out->depth == 1 || fe_globalData.force_mono_p || broken_visual)
    {
      if (colormap->contexts > 1) {
        assert(colormap->color_space);
        goto colorspace_init_complete;
      }

      /* Tell the image library to only ever return us depth 1 il_images.
	 We will still enlarge these to visual-depth when displaying them. */
      if (!context->color_space)
          context->color_space = IL_CreateGreyScaleColorSpace(1, 1);

      fe_SetTransparentPixel(context, 0xff, 0xff, 0xff, 0xff);
    }
  else if (vi_out->class == StaticGray ||
	   vi_out->class == GrayScale)
    {
      XColor color;

      if (colormap->contexts > 1) {
        assert(colormap->color_space);
        goto colorspace_init_complete;
      }

      if (!context->color_space)
          context->color_space = IL_CreateGreyScaleColorSpace(pixmap_depth,
                                                              pixmap_depth);

      color.pixel = CONTEXT_DATA (context)->default_bg_pixel;
      fe_QueryColor (context, &color);
      fe_SetTransparentPixel(context, (color.red>>8), (color.green>>8),
                             (color.blue>>8), color.pixel);
    }
  else if (vi_out->class == TrueColor ||
	   vi_out->class == DirectColor)
    {
      XColor color;
      IL_RGBBits rgb;           /* RGB bit allocation and shifts. */

      if (colormap->contexts > 1) {
          assert(colormap->color_space);
          goto colorspace_init_complete;
      }

      /* Way cool. Tell image library to use true-color mode. Inform it
         of the locations of the components and the total size of the
         components. For X we need to do some work to figure out what
         the shifts are for each component. Sigh.

	 I suppose it might be possible that some system somewhere
         interleaves the bits used in the RGB values (since the X server
         claims these are masks, not indexes) but that seems pretty unlikely.
       */
      FindShift (visual->red_mask, &rgb.red_shift, &rgb.red_bits);
      FindShift (visual->green_mask, &rgb.green_shift, &rgb.green_bits);
      FindShift (visual->blue_mask, &rgb.blue_shift, &rgb.blue_bits);

      if (!context->color_space)
          context->color_space = IL_CreateTrueColorSpace(&rgb, pixmap_depth);

      color.pixel = CONTEXT_DATA (context)->default_bg_pixel;
      fe_QueryColor (context, &color);
      fe_SetTransparentPixel(context, (color.red>>8), (color.green>>8),
                             (color.blue>>8), 0xff);
    }
  else
    {
      int max;
      XColor color;
      IL_ColorMap *cmap;

      /* Have we initialized this fe_colormap before ? */
      if (colormap->contexts > 1) {
        assert(colormap->color_space);
        goto colorspace_init_complete;
      }

      max = 256;
      /* User-specified max number of cells. */

      if (!colormap->private_p)
        {
          if ((fe_globalData.max_image_colors) > 0 && !colormap->private_p)
            max = fe_globalData.max_image_colors;

#ifdef __sgi
          if (max > 170 && fe_globalData.sgi_mode_p)
            /* When SGI "schemes" are in use, we need to leave them A LOT of
               free cells.  What a crock! */
            max = 170;
#endif /* __sgi */
        } 

      cmap = fe_RealizeDefaultColormap(context, max);

      /* If we can't get even 8 colors, revert to monochrome and tell
         the user a small lie (an exaggeration, really). */
      if (!warned && !cmap->num_colors)
	{
	  char buf [2048];
	  PR_snprintf (buf, sizeof (buf),
			fe_globalData.cube_too_small_message, 2);
	  FE_Alert (context, buf);
	  warned = True;
          colormap->contexts--;
          fe_globalData.force_mono_p = True;
          if (context->color_space)
              IL_ReleaseColorSpace(context->color_space);
          fe_InitColormap(context);
          return;
	}

      /* Warn the user if we couldn't allocate a decent number of colors */
      if (!warned && cmap->num_colors < 48 && 
	  vi_out->class == PseudoColor)
	{
	  char buf [2048];
	  PR_snprintf (buf, sizeof (buf),
                       fe_globalData.cube_too_small_message,
                       cmap->num_colors);
	  FE_Alert (context, buf);
	  warned = True;
	}

      if (!context->color_space)
          context->color_space = IL_CreatePseudoColorSpace(cmap, pixmap_depth,
                                                           pixmap_depth);
      /* Get background color */
      color.pixel = CONTEXT_DATA (context)->default_bg_pixel;
      fe_QueryColor (context, &color);
      fe_SetTransparentPixel(context, (color.red>>8), (color.green>>8),
                             (color.blue>>8), color.pixel);
    }

  colormap->color_space = context->color_space;

colorspace_init_complete:

  if (!fe_ImagesCantWork)
    {
      extern IL_DitherMode fe_pref_string_to_dither_mode (char *s);
      uint32 display_prefs;
      IL_DisplayData dpy_data;
      char* dither_images = fe_globalPrefs.dither_images;

#ifndef M12N                    /* XXXM2N Fix custom colormaps. */
      IL_ColorRenderMode color_render_mode;
      
      if (colormap->private_p)
          color_render_mode = ilInstallPaletteAllowed;
      else
          color_render_mode = ilRGBColorCube;

      IL_SetColorRenderMode (context, color_render_mode);
#endif /* M12N */

#ifndef M12N                    /* XXXM12N Get rid of this.  It never did
                                  anything. */
      IL_SetByteOrder (context,
		       BitmapBitOrder (dpy) == 0,
		       ImageByteOrder (dpy) == 0);
#endif /* M12N */

      dpy_data.color_space = context->color_space;
      dpy_data.progressive_display = fe_globalPrefs.streaming_images;
      dpy_data.dither_mode =
          fe_pref_string_to_dither_mode(dither_images);
      display_prefs = IL_COLOR_SPACE | IL_PROGRESSIVE_DISPLAY | IL_DITHER_MODE;
      IL_SetDisplayMode (context->img_cx, display_prefs, &dpy_data);
    }
}


/* Set the transparent pixel color.  The transparent pixel is passed into
   calls to IL_GetImage for image requests that do not use a mask. */
XP_Bool
fe_SetTransparentPixel(MWContext *context, uint8 red, uint8 green,
                       uint8 blue, Pixel server_index)
{
    IL_ColorSpace *color_space = context->color_space;
    IL_IRGB *trans_pixel = context->transparent_pixel;

    if (!trans_pixel) {
        trans_pixel = context->transparent_pixel = XP_NEW_ZAP(IL_IRGB);
        if (!trans_pixel)
            return FALSE;
    }

    /* Set the color of the transparent pixel. */
    trans_pixel->red = red;
    trans_pixel->green = green;
    trans_pixel->blue = blue;
    
    /* For PseudoColor and GreyScale visuals, we must also provide the Image
       Library with the index of the transparent pixel.  Note that this
       must be an index into the map array of the cross-platform IL_ColorMap,
       and not an FE palette index. */
	XP_ASSERT(color_space);
    if (color_space->type == NI_PseudoColor ||
        color_space->type == NI_GreyScale) {
        int i, index, num_colors;
        IL_ColorMap *cmap = &color_space->cmap;
        IL_RGB *map;

        num_colors = cmap->num_colors;
        map = cmap->map;

	/*
	 * I dont know why, but map needs to be valid or else bad things 
	 * happen on monochorme displays...at this point map will be 0x0 
	 * on monochrome display, but i guess the following functions assume
	 * it wont, cause there are no checks....
	 */
	if (!map || !num_colors)
	{
	    return FALSE;
	}

        for (i = 0, index = -1; i < num_colors; i++, map++) {
            if ((map->red == red) &&
                (map->green == green) &&
                (map->blue == blue)) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            /* The transparent pixel color isn't in the IL_ColorMap, so
               add it to the map array. */
            if (!IL_AddColorToColorMap(cmap, trans_pixel))
                return FALSE;

            /* We must also update the IL_ColorMap's index array. */
            cmap->index[trans_pixel->index] = (uint8)server_index;
        }
        else {
            trans_pixel->index = index;
        }
    }
            
    return TRUE;
}
