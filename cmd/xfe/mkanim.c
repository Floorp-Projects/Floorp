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
/**********************************************************************
 mkanim.c
 By Daniel Malmer

 This code was mostly taken from mkicons.c.

 Given a list of filenames, creates a datafile that is read in by the
 browser at run-time.  The images in the datafile will be used as the
 custom animation.

 Usage:

    mkanim large-file1 large-file2... large-filen small-file1 small-file2... 
           small-filen

**********************************************************************/

/*
   mkicons.c --- converting transparent GIFs to embeddable XImage data.
   Created: Jamie Zawinski <jwz@netscape.com>, 17-Aug-95.
   (Danger.  Here be monsters.)
 */

#define MAX_ANIM_FRAMES 50

#ifdef sgi
#include <bstring.h>
#else
#include <string.h>
#endif

#ifdef _HPUX_SOURCE
typedef unsigned char uchar_t;
#endif

#include "if.h"
#include "prtypes.h"
#include <plevent.h>
#include <prtypes.h>
#include "libevent.h"

extern void append_to_output_file(char* data, int len);

/* =========================================================================
   All this junk is just to get it to link.
   =========================================================================
 */

void * FE_SetTimeout(TimeoutCallbackFunction func, void * closure,
                     uint32 msecs) { return 0; }
void FE_ClearTimeout(void *timer_id) {}
void XP_Trace(const char * format, ...) {}
void FE_ImageDelete(IL_Image *portableImage) {}
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
void LO_RefreshAnchors(MWContext *context) { }
void GH_UpdateGlobalHistory(URL_Struct * URL_s) { }
char * NET_EscapeHTML(const char * string) { return (char *)string; }
Bool LO_LocateNamedAnchor(MWContext *context, URL_Struct *url_struct,
			  int32 *xpos, int32 *ypos) { return FALSE; }
Bool LO_HasBGImage(MWContext *context) {return FALSE; }

void FE_UpdateStopState(MWContext *context) {}

void
FE_Alert (MWContext *context, const char *message) {}

void ET_RemoveWindowContext(MWContext *context, ETVoidPtrFunc fn,
			    void *data) { }

extern int il_first_write(NET_StreamClass *stream, const unsigned char *str, int32 len);

void fe_ReLayout (MWContext *context, NET_ReloadMethod force_reload) {}

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
struct image images[500] = { { 0, }, };

int total_colors;
IL_IRGB cmap[256];

char* current_file = NULL;

int num_frames_small = 0;
int width_small      = 0;
int height_small     = 0;

int num_frames_large = 0;
int width_large      = 0;
int height_large     = 0;

int in_anim = 0;
int inactive_icon_p = 0;
int anim_frames[100] = { 0, };

XP_Bool sgi_p = FALSE;

static unsigned char *bitrev = 0;


/*
 * warn
 */
void
warn(char* msg)
{
    fprintf(stderr, "Warning: %s: %s\n", current_file, msg);
}


/*
 * error
 */
void
error(char* msg)
{
    fprintf(stderr, "Error: %s: %s\n", current_file, msg);

    exit(1);
}


/*
 * init_reverse_bits
 */
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


typedef enum {INITIAL_STATE, LARGE_STATE, SMALL_STATE, ERROR_STATE} image_state;


/*
 * image_size
 * This routine is registered with the image library to be invoked
 * when it has determined the size of the image that it is processing.
 * Sets the size of the image, and allocates the appropriate memory.
 */
int
image_size (MWContext *context, IL_ImageStatus message,
	    IL_Image *il_image, void *data)
{
  static image_state state = INITIAL_STATE;

  switch ( state ) {
    case INITIAL_STATE:
      printf("First large frame is %s.\n", current_file);
      printf("Size is %d by %d pixels.\n", il_image->width, il_image->height);
      num_frames_large++;
      width_large = il_image->width;
      height_large = il_image->height;
      state = LARGE_STATE;
      break;
    case LARGE_STATE:
      if ( il_image->width == width_large &&
           il_image->height == height_large ) {
        if ( num_frames_large < MAX_ANIM_FRAMES ) {
          num_frames_large++;
          printf("Reading large frame %d...\n", num_frames_large);
        } else {
          warn("Ignoring.  Too many frames for animation.");
        }
      } else {
        printf("First small frame is %s.\n", current_file);
        printf("Size is %d by %d pixels.\n", il_image->width, il_image->height);
        num_frames_small++;
        width_small = il_image->width;
        height_small = il_image->height;
        state = SMALL_STATE;
      }
      break;
    case SMALL_STATE:
      if ( il_image->width == width_small &&
           il_image->height == height_small ) {
        if ( num_frames_small < MAX_ANIM_FRAMES ) {
          num_frames_small++;
          printf("Reading small frame %d...\n", num_frames_small);
        } else {
          warn("Ignoring.  Too many frames for animation.");
        }
      } else {
        error("Bad size.  Animation frames must have the same size.\n");
        state = ERROR_STATE;
      }
      break;
    case ERROR_STATE:
      break;
    default:
      error("Unexpected error.  Reached bad switch.\n");
      break;
  }

  if (il_image->bits)
    free (il_image->bits);

  il_image->bits = malloc (il_image->widthBytes * il_image->height);
  memset (il_image->bits, ~0, (il_image->widthBytes * il_image->height));
  if (!il_image->mask && il_image->transparent)
    {
      int size = il_image->maskWidthBytes * il_image->height;
      il_image->mask = malloc (size);
      memset (il_image->mask, ~0, size);
    }

  return 0;
}


/*
 * print_image
 */
void
print_image(IL_Image* il_image)
{
    printf("---------------------------------------------\n");
    printf("il_image->width             = %d\n", il_image->width);
    printf("il_image->height            = %d\n", il_image->height);
    printf("il_image->widthBytes        = %d\n", il_image->widthBytes);
    printf("il_image->maskWidthBytes    = %d\n", il_image->maskWidthBytes);
    printf("il_image->depth             = %d\n", il_image->depth);
    printf("il_image->bytesPerPixel     = %d\n", il_image->bytesPerPixel);
    printf("il_image->colors            = %d\n", il_image->colors);
    printf("il_image->unique_colors     = %d\n", il_image->unique_colors);
    printf("il_image->validHeight       = %d\n", il_image->validHeight);
    printf("il_image->lastValidHeight   = %d\n", il_image->lastValidHeight);
    printf("il_image->has_mask          = %s\n", il_image->has_mask ? 
                                                 "True" : "False");
    printf("il_image->hasUniqueColormap = %s\n", il_image->hasUniqueColormap ? 
                                                 "True" : "False");
}


/*
 * get_color
 */
int
get_color(uchar_t r, uchar_t g, uchar_t b)
{
  int i;

#ifdef DEBUG_username
  printf("Trying to find %d %d %d... ", r, g, b);
#endif

  for ( i = 0; i < total_colors; i++ ) {
    if ( cmap[i].red   == r &&
         cmap[i].green == g &&
         cmap[i].blue  == b ) {
      return i;
    }
  }

  if ( total_colors > 64 ) {
    error("Animations can contain no more than 64 colors.\n");
  }

  cmap[total_colors].red   = r;
  cmap[total_colors].green = g;
  cmap[total_colors].blue  = b;

  total_colors++;

  return (total_colors-1);
}


/*
 * image_data
 * This routine is registered with the image library.  It is invoked
 * when the image library has image data to be processed.  The mono
 * bits, mask bits and color bits are written to the output buffer.
 */
void
image_data (MWContext *context, IL_ImageStatus message, IL_Image *il_image,
	    void *data)
{
  int row_parity;
  unsigned char *s, *m, *scanline, *mask_scanline, *end;

#ifdef DEBUG_username
  print_image(il_image);
#endif

  if (message != ilComplete) abort ();
  images[total_images].width = il_image->width;
  images[total_images].height = il_image->height;
  if (il_image->depth == 1)
    images[total_images].mono_bits = il_image->bits;
  else
    images[total_images].color_bits = il_image->bits;
  if (il_image->mask)
    images[total_images].mask_bits = il_image->mask;

  if (il_image->depth == 1)
    return;
  if (il_image->depth != 32) {
    error("Color image depth not 32.\n");
  }

  /* Generate monochrome icon from color data. */
  scanline = il_image->bits;
  mask_scanline = il_image->mask;
  end = scanline + (il_image->widthBytes * il_image->height);
  row_parity = 0;
      
  while (scanline < end)
    {
      unsigned char *scanline_end = scanline + (il_image->width * 4);
      int luminance, pixel;
      int bit = 0;
      uchar_t byte = 0;

      row_parity ^= 1;
      for (m = mask_scanline, s = scanline; s < scanline_end; s += 4)
        {
          unsigned char r = s[3];
          unsigned char g = s[2];
          unsigned char b = s[1];

          luminance = (0.299 * r) + (0.587 * g) + (0.114 * b);

          pixel =
            ((luminance < 128))                      ||
            ((r ==  66) && (g == 154) && (b == 167)); /* Magic: blue */
          byte |= pixel << bit++;

          if ((bit == 8) || ((s + 4) >= scanline_end)) {
            /* Handle transparent areas of the icon */
            if (il_image->mask)
              byte &= bitrev[*m++];

            append_to_output_file((char*) &byte, 1);

            bit = 0;
            byte = 0;
          }
        }
      scanline += il_image->widthBytes;
      mask_scanline += il_image->maskWidthBytes;
    }

  /* Mask data */
  if (il_image->mask)
    {
      scanline = il_image->mask;
      end = scanline + (il_image->maskWidthBytes * il_image->height);
      for (;scanline < end; scanline += il_image->maskWidthBytes)
        {
          unsigned char *scanline_end = scanline + ((il_image->width + 7) / 8);
          for (s = scanline; s < scanline_end; s++)
            {
              append_to_output_file((char*) &(bitrev[*s]), 1);
            }
        }
    }
  else
    {
      append_to_output_file(0, (il_image->width+7)/8*(il_image->height));
    }

  /* Color icon */

  scanline = il_image->bits;
  end = scanline + (il_image->widthBytes * il_image->height);
  for (;scanline < end; scanline += il_image->widthBytes)
    {
      unsigned char *scanline_end = scanline + (il_image->width * 4);
      for (s = scanline; s < scanline_end; s += 4)
        {
          unsigned char r = s[3];
          unsigned char g = s[2];
          unsigned char b = s[1];
          int j;
          uchar_t byte;
          j = get_color(r, g, b);
          byte = (uchar_t) j;
          append_to_output_file((char*) &byte, 1);

#ifdef DEBUG_username
          for (j = 0; j < total_colors; j++)
            if (r == cmap[j].red &&
                g == cmap[j].green &&
                b == cmap[j].blue)
              {
                byte = (uchar_t) j;
                append_to_output_file((char*) &byte, 1);
                goto DONE;
              }
          error("Error allocated colors.\n");
        DONE:
          ;
#endif
        }
    }

  if (il_image->bits) free (il_image->bits);
  il_image->bits = 0;
  if (il_image->mask) free (il_image->mask);
  il_image->mask = 0;
}


/*
 * set_title
 * A no-op.
 */
void
set_title (MWContext *context, char *title)
{
}


/*
 * do_file
 * Process an image file.
 */
void
do_file (char *file)
{
  static int counter = 0;
  FILE *fp;
  char *data;
  int size;
  NET_StreamClass *stream;
  struct stat st;
  char *s;

  URL_Struct *url;
  il_container *ic;
  il_process *proc;
  MWContext cx;
  ContextFuncs fns;
  IL_IRGB trans;

  memset (&cx, 0, sizeof(cx));
  memset (&fns, 0, sizeof(fns));
  
  url = XP_NEW_ZAP (URL_Struct);
  proc = XP_NEW_ZAP (il_process);

  url->address = strdup("\000\000");
  cx.funcs = &fns;

  fns.ImageSize = image_size;
  fns.ImageData = image_data;
  fns.SetDocTitle = set_title;

  {
    /* make a container */
    ic = XP_NEW_ZAP(il_container);
    ic->hash = 0;
    ic->urlhash = 0;
    ic->cx = &cx;
    ic->forced = 0;

    ic->image = XP_NEW_ZAP(IL_Image);

    ic->next = 0;
    ic->ip = proc;
    cx.imageProcess = proc;

    ic->state = IC_START;
  }

  url->fe_data = ic;

  ic->clients = XP_NEW_ZAP(il_client);
  ic->clients->cx = &cx;

  if ( stat (file, &st) ) {
    perror(file);
    exit(errno);
  }

  if ( !S_ISREG(st.st_mode) ) {
    fprintf(stderr, "%s:  Not a plain file.\n", file);
    exit(1);
  }

  size = st.st_size;

  data = (char *) malloc (size + 1);
  fp = fopen (file, "r");
  fread (data, 1, size, fp);
  fclose (fp);

  current_file = strdup(file);

  s = strrchr (file, '.');
  if (s) *s = 0;
  s = strrchr (file, '/');
  if (s)
    s++;
  else
    s = file;

  if (in_anim && strncmp (s, "Anim", 4))
    /* once you've started anim frames, don't stop. */
    abort ();

  if ((!strcmp (s, "AnimSm00") || !strcmp (s, "AnimHuge00")) ||
      ((!strcmp (s, "AnimSm01") || !strcmp (s, "AnimHuge01")) &&
       (!in_anim ||
	anim_frames[in_anim-1] > 1)))
    {
      char *s2;

      s2 = s - 2;
      while (s2 > file && *s2 != '/')
	s2--;
      s[-1] = 0;
      if (*s2 == '/')
	s2++;

      in_anim++;
    }

  if (in_anim)
    {
      if (strncmp (s, "Anim", 4)) abort ();
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
    }

  trans.red = trans.green = trans.blue = 0xC0;

  cx.colorSpace = NULL;

  IL_EnableTrueColor (&cx, 32,
#if defined(IS_LITTLE_ENDIAN)
		      24, 16, 8,
#elif defined(IS_BIG_ENDIAN)
		      0, 8, 16,
#else
  ERROR!  Endianness unknown.
#endif
		      8, 8, 8,
		      &trans, FALSE);

  ic->cs = cx.colorSpace;
  /*IL_SetPreferences (&cx, FALSE, ilClosestColor);*/
  IL_SetPreferences (&cx, FALSE, ilDither);/* XXXM12N Replace with
                                             IL_SetDisplayMode. */
  url->address[0] = counter++;
  stream = IL_NewStream (FO_PRESENT,
			 (strstr (current_file, ".gif") ? (void *) IL_GIF :
			  strstr (current_file, ".jpg") ? (void *) IL_JPEG :
			  strstr (current_file, ".jpeg") ? (void *) IL_JPEG :
			  strstr (current_file, ".xbm") ? (void *) IL_XBM :
			  (void *) IL_GIF),
			 url, &cx);
  if ( stream->put_block (stream, data, size) < 0 ) {
    error("Couldn't decode file.\n");
  }

  stream->complete (stream);

  free(current_file);
  free (data);

  total_images++;
}


uchar_t* output_buf = NULL;
int output_max_len = 0;
int output_len = 0;


/*
 * CHUNKSIZE should be 1024 or something, but make
 * it small to make sure it works.
 */
#define CHUNKSIZE 64


/*
 * append_to_output_file
 * Append the given data to the output buffer.  If the resulting data
 * is too large for the currently allocated output buffer, first allocate
 * additional space.
 */
void
append_to_output_file(char* data, int len)
{
    while ( output_buf == NULL || output_len + len >= output_max_len ) {
        output_max_len+= CHUNKSIZE;
        output_buf = (char*) realloc(output_buf, output_max_len);
    }
 
    if ( data ) {
      memcpy(output_buf+output_len, data, len);
    } else {
      memset(output_buf+output_len, 0xff, len);
    }

    output_len+= len;
}


#define ANIMATION_FILENAME "animation.dat"


/*
 * usage
 */
void
usage(char* progname)
{
    fprintf(stderr, "Usage: %s large-animation-files small-animation-files\n",
                    progname);
}


/*
 * main
 * Process each file given on the command line.
 */
int
main (int argc, char* argv[])
{
  int i;
  FILE* anim_file;

  init_reverse_bits();

  if ( argc < 2 ) {
    usage(argv[0]);
    exit(1);
  }

  for (i = 1; i < argc; i++) {
    char *filename = argv[i];
    inactive_icon_p = (strstr(filename, ".i.gif") != NULL);
    do_file (filename);
  }

  if ( (anim_file = fopen(ANIMATION_FILENAME, "w")) == NULL ) {
    perror(ANIMATION_FILENAME);
    exit(errno);
  }

  fprintf(anim_file, "%d %d %d\n", num_frames_large, width_large, height_large);
  fprintf(anim_file, "%d %d %d\n", num_frames_small, width_small, height_small);
  fprintf(anim_file, "%d\n", total_colors);

  for ( i = 0; i < total_colors; i++ ) {
    fprintf(anim_file, "%x %x %x\n", cmap[i].red * 257,
                                     cmap[i].green * 257,
                                     cmap[i].blue * 257);
  }

  if ( output_buf == NULL ) {
    error("No image data to write.\n");
  }

  if ( fwrite(output_buf, sizeof(uchar_t), output_len, anim_file) != 
        output_len) {
    perror(ANIMATION_FILENAME);
    exit(errno);
  }

  if ( fclose(anim_file) != 0 ) {
    perror(ANIMATION_FILENAME);
    exit(errno);
  }

  printf("Wrote %d large frames, %d small frames.\n", num_frames_large, 
                                                      num_frames_small);

  printf("%d colors\n", total_colors);

  fprintf(stderr, "Successfully wrote '%s'\n", ANIMATION_FILENAME);

  return 0;
}


