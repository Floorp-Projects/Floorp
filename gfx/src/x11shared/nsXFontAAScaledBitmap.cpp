/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Stell <bstell@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <X11/Xatom.h>
#include "gfx-config.h"
#include "nscore.h"
#include "nsXFontAAScaledBitmap.h"
#include "nsRenderingContextGTK.h"
#include "nsX11AlphaBlend.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "nsHashtable.h"

#define IMAGE_BUFFER_SIZE 2048

#ifdef DEBUG
  static PRBool dodump = 0;
# define DEBUG_DUMP(x) if (dodump) {dodump=0; x; }
#else
# define DEBUG_DUMP(x)
#endif

#define DEBUG_SHOW_GLYPH_BOX 0
#if DEBUG_SHOW_GLYPH_BOX
# define DEBUG_AADRAWBOX(i,x,y,w,h,r,g,b,a) \
    PR_BEGIN_MACRO \
      nscolor color NS_RGB((r),(g),(b)); \
      AADrawBox((i), (x), (y), (w), (h), color, (a)); \
    PR_END_MACRO
#else
# define DEBUG_AADRAWBOX(i,x,y,w,h,r,g,b,a)
#endif

void dump_byte_table(PRUint8 *table, int width, int height);

static void scale_image(nsAntiAliasedGlyph *, nsAntiAliasedGlyph *);
static void scale_imageAntiJag(nsAntiAliasedGlyph *, nsAntiAliasedGlyph *);
static void WeightTableInitLinearCorrection(PRUint8*, PRUint8, double);

Display *nsXFontAAScaledBitmap::sDisplay       = nsnull;
GC       nsXFontAAScaledBitmap::sBackgroundGC  = nsnull;
PRUint8  nsXFontAAScaledBitmap::sWeightedScaleDarkText[256];
PRUint8  nsXFontAAScaledBitmap::sWeightedScaleLightText[256];

//
// Anti-Aliased Scaled Bitmap Fonts (AASB)
// 
// AASB fonts are made by taking a regular X font, drawing the desired
// glyph(s) off screen, bringing the pixels down to the client, and then
// scaling the bits using anti-aliasing to get a correctly weighted
// pixels in the scaled version. The AASB glyphs are visually correct 
// but tend to be a bit weak and blurry. 
//
// To make the glyphs stronger and sharper the pixels are adjusted.
//
// One strategy would be to lighten the light pixels and darken the 
// dark pixels. Lightening the light pixels makes "thin" lines
// disappear completely so this is not done.
//
// To darken the dark pixels the code increases the value of the pixels
// (above a minimum value).
//
//  for (i=0; i<256; i++) {
//    if (i < min_value)
//      val = i;
//    else
//      val = i + ((i - min_value)*gain);
//
// (of course limiting val to between 0 and 255 inclusive)
//
// Dark text (eg: black glyphs on a white background) is weighted
// separately from light text (eg: white text on a blue background).
// Selected text is white text on a blue background. The contrast
// between white text on a blue background is not as strong a black
// black text on a white background so light colored text is
// "sharpened" more than dark text.

PRUint8 gAASBDarkTextMinValue = 64;
double gAASBDarkTextGain = 0.6;
PRUint8 gAASBLightTextMinValue = 64;
double gAASBLightTextGain = 1.3;

PRBool
nsXFontAAScaledBitmap::DisplayIsLocal(Display *aDisplay)
{
  // if shared memory works then the display is local
  if (gdk_get_use_xshm())
    return PR_TRUE;

  return PR_FALSE;

}

void
nsXFontAAScaledBitmap::DrawText8(GdkDrawable *aDrawable, GdkGC *aGC,
                                 PRInt32 aX, PRInt32 aY,
                                 const char *aString, PRUint32 aLength)
{
  DrawText8or16(aDrawable, aGC, aX, aY, (void*)aString, aLength);
}

void
nsXFontAAScaledBitmap::DrawText16(GdkDrawable *aDrawable, GdkGC *aGC,
                         PRInt32 aX, PRInt32 aY,
                        const XChar2b *aString, PRUint32 aLength)
{
  DrawText8or16(aDrawable, aGC, aX, aY, (void*)aString, aLength);
}

//
// We save code space by merging the 8 and 16 bit drawing routines.
// The signature of this fuction is vague but that is okay since
// it is only used internally by nsXFontAAScaledBitmap and not
// presented as an API.
//
void
nsXFontAAScaledBitmap::DrawText8or16(GdkDrawable *aDrawable, GdkGC *aGC,
                                     PRInt32 aX, PRInt32 aY,
                                     void *a8or16String, PRUint32 aLength)
{
  // make the indeterminate input variables determinate
  const char    *string8  = (const char    *)a8or16String;
  const XChar2b *string16 = (const XChar2b *)a8or16String;

#if DEBUG_SHOW_GLYPH_BOX
  // grey shows image size
  // red shows character cells
  // green box shows text ink
#endif

  if (aLength < 1) {
    return;
  }

  //
  // Get a area guaranteed to be big enough.
  //
  // This can be bigger than necessary but for debuggability
  // this code can draw the char cells.  Calculating the
  // exact size needed both the character cells and
  // pixels to the left and right of the character cells
  // is quite messy.
  //
  PRUint32 image_width  = (mScaledMax.width * aLength) + mScaledMax.lbearing;
  PRUint32 image_height = mScaledMax.ascent+mScaledMax.descent;
  PRInt32 x_pos = mScaledMax.lbearing;

  Drawable win = GDK_WINDOW_XWINDOW(aDrawable);
  GC gc = GDK_GC_XGC(aGC);
  XGCValues values;
  if (!XGetGCValues(mDisplay, gc, GCForeground, &values)) {
    NS_ASSERTION(0, "failed to get foreground pixel");
    return;
  }
  nscolor color = nsX11AlphaBlend::PixelToNSColor(values.foreground);

  // weight dark text differently from light text
  PRUint32 color_val = NS_GET_R(color) + NS_GET_G(color) + NS_GET_B(color);
  PRUint8 *weight_table;
  if ((NS_GET_R(color)>200) || (NS_GET_R(color)>200) || (NS_GET_R(color)>200)
      || (color_val>3*128))
    weight_table = sWeightedScaleLightText;
  else
    weight_table = sWeightedScaleDarkText;

  //
  // Get the background
  //
  XImage *sub_image = nsX11AlphaBlend::GetBackground(mDisplay, mScreen, win,
                                 aX-mScaledMax.lbearing, aY-mScaledMax.ascent,
                                 image_width, image_height);
  if (sub_image==nsnull) {
#ifdef DEBUG
    // complain if the requested area is not completely off screen
    int win_width = DisplayWidth(mDisplay, mScreen);
    int win_height = DisplayHeight(mDisplay, mScreen);
    if (((int)(aX-mScaledMax.lbearing+image_width) > 0)  // not hidden to left
        && ((int)(aX-mScaledMax.lbearing) < win_width)   // not hidden to right
        && ((int)(aY-mScaledMax.ascent+image_height) > 0)// not hidden to top
        && ((int)(aY-mScaledMax.ascent) < win_height))   // not hidden to bottom
    {
      NS_ASSERTION(sub_image, "failed to get the image");
    }
#endif
    return;
  }

#if DEBUG_SHOW_GLYPH_BOX
  DEBUG_AADRAWBOX(sub_image,0,0,image_width,image_height,0,0,0,255/4);
  int lbearing, rbearing, width, ascent, descent;
  if (mIsSingleByte)
    TextExtents8(aString8, aLength, &lbearing, &rbearing, &width,
                 &ascent, &descent);
  else
    TextExtents16(aString16, aLength, &lbearing, &rbearing, &width,
                  &ascent, &descent);

  DEBUG_AADRAWBOX(sub_image, x_pos+lbearing, mScaledMax.ascent-ascent,
                  rbearing-lbearing, ascent+descent, 0,255,0, 255/2);
#endif

  //
  // Get aa-scaled glyphs and blend with background
  //
  blendGlyph blendGlyph = nsX11AlphaBlend::GetBlendGlyph();
  for (PRUint32 i=0; i<aLength; i++) {
#if DEBUG_SHOW_GLYPH_BOX
    PRUint32 box_width;
    if (mIsSingleByte)
      box_width = TextWidth8(&aString8[i], 1);
    else
      box_width = TextWidth16(&aString16[i], 1);
    nscolor red = NS_RGB(255,0,0);
    AADrawBox(sub_image, x_pos, 0, box_width, mScaledMax.ascent, red, 255/4);
    AADrawBox(sub_image, x_pos, mScaledMax.ascent, box_width,
              mScaledMax.descent, red,255/4);
#endif
    nsAntiAliasedGlyph *scaled_glyph;
    PRBool got_image;
    if (mIsSingleByte)
      got_image = GetScaledGreyImage(&string8[i], &scaled_glyph);
    else
      got_image = GetScaledGreyImage((const char*)&string16[i], &scaled_glyph);
    if (!got_image) {
      PRUint32 char_width;
      if (mIsSingleByte)
        char_width = XTextWidth(mUnscaledFontInfo, &string8[i], 1);
      else
        char_width = XTextWidth16(mUnscaledFontInfo, &string16[i], 1);
      x_pos += SCALED_SIZE(char_width);
      continue;
    }
    NS_ASSERTION(scaled_glyph->GetBorder()==0,
                 "do not support non-zero border");
    DEBUG_DUMP((dump_byte_table(scaled_glyph->GetBuffer(),
                    scaled_glyph->GetBufferWidth(),
                    scaled_glyph->GetBufferHeight())));
    //
    // blend the aa-glyph onto the background
    //
    (*blendGlyph)(sub_image, scaled_glyph, weight_table, color,
                  x_pos + scaled_glyph->GetLBearing(), 0);

    DEBUG_DUMP((dump_XImage_blue_data(sub_image)));
    x_pos += scaled_glyph->GetAdvance();
  }
  DEBUG_DUMP((dump_XImage_blue_data(sub_image)));

  //
  // Send it to the display
  //
  XPutImage(mDisplay, win, gc, sub_image,
            0, 0, aX-mScaledMax.lbearing, aY-mScaledMax.ascent,
            image_width, image_height);
  XDestroyImage(sub_image);
}

void
nsXFontAAScaledBitmap::FreeGlobals()
{
  if (sBackgroundGC) {
    XFreeGC(sDisplay, sBackgroundGC);
    sBackgroundGC = nsnull;
  }
  sDisplay = nsnull;
}

static PRBool
FreeGlyphHash(nsHashKey* aKey, void* aData, void* aClosure)
{
  delete (nsAntiAliasedGlyph *)aData;

  return PR_TRUE;
}

PRBool
nsXFontAAScaledBitmap::GetScaledGreyImage(const char *aChar,
                                         nsAntiAliasedGlyph **aGreyImage)
{
  XChar2b *aChar2b = nsnull;
  PRUint32 antiJagPadding;
  XImage *ximage;
  nsAntiAliasedGlyph *scaled_image;
  PRUnichar charKey[2];

  // get the char key
  if (mIsSingleByte)
    charKey[0] = (PRUnichar)*aChar;
  else {
    aChar2b = (XChar2b *)aChar;
    charKey[0] = aChar2b->byte1<<8 | aChar2b->byte2;
  }
  charKey[1] = 0;
  nsStringKey key(charKey, 1);

  // look in the cache for the glyph image
  scaled_image = (nsAntiAliasedGlyph*)mGlyphHash->Get(&key);
  if (!scaled_image) {
    // get the char metrics
    int direction, font_ascent, font_descent;
    XCharStruct charMetrics;
    if (mIsSingleByte)
      XTextExtents(mUnscaledFontInfo, aChar, 1, &direction,
                   &font_ascent, &font_descent, &charMetrics);
    else
      XTextExtents16(mUnscaledFontInfo, aChar2b,1, &direction,
                     &font_ascent, &font_descent, &charMetrics);

    // figure the amount to scale
    PRInt32 left_edge  = GLYPH_LEFT_EDGE(&charMetrics);
    PRInt32 right_edge = GLYPH_RIGHT_EDGE(&charMetrics);
    PRUint32 unscaled_width = right_edge - left_edge;
    NS_ASSERTION(unscaled_width<=mUnscaledMax.width, "unexpected glyph width");

    // clear the bitmap
    XFillRectangle(mDisplay, mUnscaledBitmap, sBackgroundGC, 0, 0,
                                unscaled_width, mUnscaledMax.height);
    // draw the char
    if (mIsSingleByte)
      XDrawString(mDisplay, mUnscaledBitmap, mForegroundGC,
               -left_edge, mUnscaledMax.ascent, aChar, 1);
    else
      XDrawString16(mDisplay, mUnscaledBitmap, mForegroundGC,
               -left_edge, mUnscaledMax.ascent, aChar2b, 1);
    // get the pixels
    ximage = XGetImage(mDisplay, mUnscaledBitmap,
                     0, 0, unscaled_width, mUnscaledMax.height,
                     AllPlanes, ZPixmap);
    NS_ASSERTION(ximage, "failed to XGetSubImage()");
    if (!ximage) {
      return PR_FALSE;
    }
    DEBUG_DUMP((dump_XImage_blue_data(ximage)));

    // must pad when anti-jagging
    if (mRatio < 1.25)
      antiJagPadding = 0;
    else
      antiJagPadding = 2; // this may change if the anti-jagging code changes

    // create the empty nsAntiAliasedGlyph
    nsAntiAliasedGlyph unscaled_image(unscaled_width, mUnscaledMax.height,
                                      antiJagPadding);
    PRUint8 buf[IMAGE_BUFFER_SIZE]; // try to use the stack for data
    if (!unscaled_image.Init(buf, IMAGE_BUFFER_SIZE)) {
      NS_ASSERTION(0, "failed to Init() unscaled_image");
      XDestroyImage(ximage);
      return PR_FALSE;
    }

    //
    // get ready to scale
    //
    unscaled_image.SetImage(&charMetrics, ximage);
    DEBUG_DUMP((dump_byte_table(unscaled_image.GetBuffer(),
                    unscaled_image.GetBufferWidth(),
                    unscaled_image.GetBufferHeight())));
    XDestroyImage(ximage);

    //
    // Create the scaled glyph
    //
    GlyphMetrics glyphMetrics;
    glyphMetrics.width    = SCALED_SIZE(unscaled_width);
    glyphMetrics.height   = SCALED_SIZE(mUnscaledMax.height);
    glyphMetrics.lbearing = SCALED_SIZE(left_edge);
    glyphMetrics.rbearing = SCALED_SIZE(right_edge);
    glyphMetrics.advance  = SCALED_SIZE(charMetrics.width);
    glyphMetrics.ascent   = SCALED_SIZE(charMetrics.ascent);
    glyphMetrics.descent  = SCALED_SIZE(charMetrics.descent);

    scaled_image = new nsAntiAliasedGlyph(SCALED_SIZE(unscaled_width),
                                          SCALED_SIZE(mUnscaledMax.height), 0);
    NS_ASSERTION(scaled_image, "failed to create scaled_image");
    if (!scaled_image) {
      return PR_FALSE;
    }
    if (!scaled_image->Init()) {
      NS_ASSERTION(0, "failed to create scaled_image");
      return PR_FALSE;
    }
    scaled_image->SetSize(&glyphMetrics);

    //
    // scale
    //
    if (antiJagPadding==0)
      scale_image(&unscaled_image, scaled_image);
    else
      scale_imageAntiJag(&unscaled_image, scaled_image);

    DEBUG_DUMP((dump_byte_table(scaled_image->GetBuffer(),
                    scaled_image->GetBufferWidth(),
                    scaled_image->GetBufferHeight())));

    // store in hash
    mGlyphHash->Put(&key, scaled_image);
  }
  *aGreyImage = scaled_image;
  return PR_TRUE;
}

PRBool
nsXFontAAScaledBitmap::GetXFontProperty(Atom aAtom, unsigned long *aValue)
{
  unsigned long val;
  PRBool rslt = ::XGetFontProperty(mUnscaledFontInfo, aAtom, &val);
  if (!rslt)
    return PR_FALSE;

  switch (aAtom) {
    case XA_X_HEIGHT:
      if (val >= 0x00ffffff) {// Bug 43214: arbitrary to exclude garbage values
        return PR_FALSE;
      }
    case XA_SUBSCRIPT_Y:
    case XA_SUPERSCRIPT_Y:
    case XA_UNDERLINE_POSITION:
    case XA_UNDERLINE_THICKNESS:
      *aValue = (unsigned long)SCALED_SIZE(val);
      break;
    default:
      *aValue = val;
  }
  return rslt;
}

XFontStruct *
nsXFontAAScaledBitmap::GetXFontStruct()
{
  NS_ASSERTION(mGdkFont, "GetXFontStruct called before font loaded");
  if (mGdkFont==nsnull)
    return nsnull;

  return &mScaledFontInfo;
}

PRBool
nsXFontAAScaledBitmap::InitGlobals(Display *aDisplay, int aScreen)
{
  Window root_win;

  sDisplay = aDisplay; // used to free shared sBackgroundGC

  // if not a local display then might be slow so don't run
  if (!DisplayIsLocal(aDisplay)) {
    goto cleanup_and_return;
  }

  root_win = RootWindow(sDisplay, aScreen);
  sBackgroundGC = XCreateGC(sDisplay, root_win, 0, NULL);
  NS_ASSERTION(sBackgroundGC, "failed to create sBackgroundGC");
  if (!sBackgroundGC) {
    goto cleanup_and_return;
  }
  XSetForeground(sDisplay, sBackgroundGC, 0);

  WeightTableInitLinearCorrection(sWeightedScaleDarkText,
                                  gAASBDarkTextMinValue, gAASBDarkTextGain);
  WeightTableInitLinearCorrection(sWeightedScaleLightText,
                                  gAASBLightTextMinValue, gAASBLightTextGain);
  return PR_TRUE;

cleanup_and_return:
  if (sBackgroundGC) {
    XFreeGC(sDisplay, sBackgroundGC);
    sBackgroundGC = nsnull;
  }

  return PR_FALSE;
}

PRBool
nsXFontAAScaledBitmap::LoadFont()
{
  NS_ASSERTION(!mAlreadyLoaded, "LoadFont called more than once");
  mAlreadyLoaded = PR_TRUE;

  if (!mGdkFont)
    return PR_FALSE;
  mUnscaledFontInfo = (XFontStruct *)GDK_FONT_XFONT(mGdkFont);

  XFontStruct *usfi = mUnscaledFontInfo;
  XFontStruct *sfi  = &mScaledFontInfo;

  mIsSingleByte = (usfi->min_byte1 == 0) && (usfi->max_byte1 == 0);

  mUnscaledMax.width    = MAX(usfi->max_bounds.rbearing,usfi->max_bounds.width);
  mUnscaledMax.width   -= MIN(usfi->min_bounds.lbearing, 0);
  mUnscaledMax.height   = usfi->max_bounds.ascent   + usfi->max_bounds.descent;
  mUnscaledMax.lbearing = usfi->max_bounds.lbearing;
  mUnscaledMax.rbearing = usfi->max_bounds.rbearing;
  mUnscaledMax.advance  = usfi->max_bounds.width;
  mUnscaledMax.ascent   = usfi->max_bounds.ascent;
  mUnscaledMax.descent  = usfi->max_bounds.descent;

  mScaledMax.width    = SCALED_SIZE(mUnscaledMax.width);
  mScaledMax.lbearing = SCALED_SIZE(mUnscaledMax.lbearing);
  mScaledMax.rbearing = SCALED_SIZE(mUnscaledMax.rbearing);
  mScaledMax.advance  = SCALED_SIZE(mUnscaledMax.width);
  mScaledMax.ascent   = SCALED_SIZE(mUnscaledMax.ascent);
  mScaledMax.descent  = SCALED_SIZE(mUnscaledMax.ascent + mUnscaledMax.descent)
                        - SCALED_SIZE(mUnscaledMax.ascent);
  mScaledMax.height   = mScaledMax.ascent + mScaledMax.descent;

  //
  // Scale the XFontStruct
  //
  sfi->fid               = 0;
  sfi->direction         = usfi->direction;
  sfi->min_char_or_byte2 = usfi->min_char_or_byte2;
  sfi->max_char_or_byte2 = usfi->max_char_or_byte2;
  sfi->min_byte1         = usfi->min_byte1;
  sfi->max_byte1         = usfi->max_byte1;
  sfi->all_chars_exist   = usfi->all_chars_exist;
  sfi->default_char      = usfi->default_char;
  sfi->n_properties      = 0;
  sfi->properties        = nsnull;
  sfi->ext_data          = nsnull;

  sfi->min_bounds.lbearing = SCALED_SIZE(usfi->min_bounds.lbearing);
  sfi->min_bounds.rbearing = SCALED_SIZE(usfi->min_bounds.rbearing);
  sfi->min_bounds.width    = SCALED_SIZE(usfi->min_bounds.width);
  sfi->min_bounds.ascent   = SCALED_SIZE(usfi->min_bounds.ascent);
  sfi->min_bounds.descent  =
            SCALED_SIZE(usfi->min_bounds.ascent + usfi->min_bounds.descent)
            - SCALED_SIZE(usfi->min_bounds.ascent);
  sfi->min_bounds.attributes = usfi->min_bounds.attributes;

  sfi->max_bounds.lbearing = SCALED_SIZE(usfi->max_bounds.lbearing);
  sfi->max_bounds.rbearing = SCALED_SIZE(usfi->max_bounds.rbearing);
  sfi->max_bounds.width    = SCALED_SIZE(usfi->max_bounds.width);
  sfi->max_bounds.ascent   = SCALED_SIZE(usfi->max_bounds.ascent);
  sfi->max_bounds.descent  =
            SCALED_SIZE(usfi->max_bounds.ascent + usfi->max_bounds.descent)
            - SCALED_SIZE(usfi->max_bounds.ascent);
  sfi->max_bounds.attributes = usfi->max_bounds.attributes;

  sfi->per_char = nsnull;
  sfi->ascent   = SCALED_SIZE(usfi->ascent);
  sfi->descent  = SCALED_SIZE(usfi->descent);

  //
  // need a unique foreground GC for each font size/face
  // to use to draw the unscaled bitmap font
  //
  mForegroundGC = XCreateGC(mDisplay, RootWindow(mDisplay, mScreen), 0, NULL);
  NS_ASSERTION(mForegroundGC, "failed to create mForegroundGC");
  if (!mForegroundGC) {
    goto cleanup_and_return;
  }

  XSetFont(mDisplay, mForegroundGC, usfi->fid);
  XSetForeground(mDisplay, mForegroundGC, 0xFFFFFF);

  mUnscaledBitmap = XCreatePixmap(mDisplay,
                          RootWindow(mDisplay,DefaultScreen(mDisplay)),
                          mUnscaledMax.width, mUnscaledMax.height,
                          DefaultDepth(mDisplay, mScreen));
  if (!mUnscaledBitmap)
    goto cleanup_and_return;

  mGlyphHash = new nsHashtable();
  if (!mGlyphHash)
    goto cleanup_and_return;

  if (mGdkFont) {
#ifdef NS_FONT_DEBUG_LOAD_FONT
    if (gFontDebug & NS_FONT_DEBUG_LOAD_FONT) {
      printf("loaded %s\n", mName);
    }
#endif
    return PR_TRUE;
  }
  else
    return PR_FALSE;

cleanup_and_return:
  if (mUnscaledFontInfo) {
    mUnscaledFontInfo = nsnull;
  }
  if (mForegroundGC) {
    XFreeGC(mDisplay, mForegroundGC);
    mForegroundGC = nsnull;
  }
  if (mUnscaledBitmap) {
    XFreePixmap(mDisplay, mUnscaledBitmap);
    mUnscaledBitmap = nsnull;
  }
  if (mGlyphHash) {
    delete mGlyphHash;
    mGlyphHash = nsnull;
  }
  memset(&mScaledFontInfo, 0, sizeof(mScaledFontInfo));
  memset(&mUnscaledMax,    0, sizeof(mUnscaledMax));
  memset(&mScaledMax,      0, sizeof(mScaledMax));
  return PR_FALSE;
}

nsXFontAAScaledBitmap::nsXFontAAScaledBitmap(Display *aDisplay,
                                             int aScreen,
                                             GdkFont *aGdkFont,
                                             PRUint16 aSize,
                                             PRUint16 aUnscaledSize)
{
  mAlreadyLoaded       = PR_FALSE;
  mDisplay             = aDisplay;
  mScreen              = aScreen;
  mGdkFont             = ::gdk_font_ref(aGdkFont);
  mUnscaledSize        = aUnscaledSize;
  mRatio               = ((double)aSize)/((double)aUnscaledSize);
  mIsSingleByte        = 0;
  mForegroundGC        = nsnull;
  mGlyphHash           = nsnull;
  mUnscaledBitmap      = nsnull;
  memset(&mScaledFontInfo, 0, sizeof(mScaledFontInfo));
  memset(&mUnscaledMax,    0, sizeof(mUnscaledMax));
  memset(&mScaledMax,      0, sizeof(mScaledMax));
}

void
nsXFontAAScaledBitmap::TextExtents8(const char *aString, PRUint32 aLength,
                                    PRInt32* aLBearing, PRInt32* aRBearing,
                                    PRInt32* aWidth, PRInt32* aAscent,
                                    PRInt32* aDescent)
{
  TextExtents8or16((void *)aString, aLength, aLBearing, aRBearing, aWidth,
                   aAscent, aDescent);
}

void
nsXFontAAScaledBitmap::TextExtents16(const XChar2b *aString, PRUint32 aLength,
                            PRInt32* aLBearing, PRInt32* aRBearing,
                            PRInt32* aWidth, PRInt32* aAscent,
                            PRInt32* aDescent)
{
  TextExtents8or16((void *)aString, aLength, aLBearing, aRBearing, aWidth,
                   aAscent, aDescent);
}

//
// We save code space by merging the 8 and 16 bit text extents routines.
// The signature of this fuction is vague but that is okay since
// it is only used internally by nsXFontAAScaledBitmap and not
// presented as an API.
//
void
nsXFontAAScaledBitmap::TextExtents8or16(void *a8or16String, PRUint32 aLength,
                            PRInt32* aLBearing, PRInt32* aRBearing,
                            PRInt32* aWidth, PRInt32* aAscent,
                            PRInt32* aDescent)
{
  // make the indeterminate input variables determinate
  const char    *string8  = (const char    *)a8or16String;
  const XChar2b *string16 = (const XChar2b *)a8or16String;

  int dir, unscaled_ascent, unscaled_descent;
  XCharStruct char_metrics;
  int leftBearing  = 0;
  int rightBearing = 0;
  int width        = 0;
  int ascent       = 0;
  int descent      = 0;

  // initialize the values
  if (aLength >= 1) {
    if (mIsSingleByte)
      XTextExtents(mUnscaledFontInfo, string8++, 1,
                     &dir, &unscaled_ascent, &unscaled_descent, &char_metrics);
    else
      XTextExtents16(mUnscaledFontInfo, string16++, 1,
                     &dir, &unscaled_ascent, &unscaled_descent, &char_metrics);
    leftBearing  = SCALED_SIZE(char_metrics.lbearing);
    rightBearing = SCALED_SIZE(char_metrics.rbearing);
    ascent       = SCALED_SIZE(char_metrics.ascent);
    descent      = SCALED_SIZE(mUnscaledMax.ascent+char_metrics.descent)
                   - SCALED_SIZE(mUnscaledMax.ascent);
    width        = SCALED_SIZE(char_metrics.width);
  }

  //
  // Must go char by char to handle float->int rounding
  // of the x position. If this is not done then when selecting
  // (highlighting) text the selection x pos can differ
  // which can make the text move around as more/less is selected
  //
  for (PRUint32 i=1; i<aLength; i++) {
    if (mIsSingleByte)
      XTextExtents(mUnscaledFontInfo, string8++, 1,
                   &dir, &unscaled_ascent, &unscaled_descent, &char_metrics);
    else
      XTextExtents16(mUnscaledFontInfo, string16++, 1,
                   &dir, &unscaled_ascent, &unscaled_descent, &char_metrics);
    //
    // In theory the second (or later) char may have a
    // lbearing more to the left than the first char.
    // Similarly for the rbearing.
    //
    leftBearing  = MIN(leftBearing,  width+SCALED_SIZE(char_metrics.lbearing));
    rightBearing = MAX(rightBearing, width+SCALED_SIZE(char_metrics.rbearing));
    ascent       = MAX(ascent,  SCALED_SIZE(char_metrics.ascent));
    descent      = MAX(descent,
                       SCALED_SIZE(mUnscaledMax.ascent+char_metrics.descent)
                       - SCALED_SIZE(mUnscaledMax.ascent));
    width        += SCALED_SIZE(char_metrics.width);
  }
  *aLBearing     = leftBearing;
  *aRBearing     = rightBearing;
  *aWidth        = width;
  *aAscent       = ascent;
  *aDescent      = descent;
}

PRInt32
nsXFontAAScaledBitmap::TextWidth8(const char *aString, PRUint32 aLength)
{
  int width = 0;
  // figure the width calculating all the per-char rounding
  for (PRUint32 i=0; i<aLength; i++) {
    int unscaled_width = XTextWidth(mUnscaledFontInfo, aString+i, 1);
    width += SCALED_SIZE(unscaled_width);
  }

  return width;
}

PRInt32
nsXFontAAScaledBitmap::TextWidth16(const XChar2b *aString, PRUint32 aLength)
{
  int width = 0;
  // figure the width calculating all the per-char rounding
  for (PRUint32 i=0; i<aLength; i++) {
    int unscaled_width = XTextWidth16(mUnscaledFontInfo, aString+i, 1);
    width += SCALED_SIZE(unscaled_width);
  }
  return width;
}

void
nsXFontAAScaledBitmap::UnloadFont()
{
  NS_ASSERTION(mGdkFont, "UnloadFont called when font not loaded");
  delete this;
}

nsXFontAAScaledBitmap::~nsXFontAAScaledBitmap()
{
  if (mGlyphHash) {
    mGlyphHash->Reset(FreeGlyphHash, nsnull);
    delete mGlyphHash;
  }
  if (mForegroundGC) {
    XFreeGC(mDisplay, mForegroundGC);
  }
  if (mGdkFont) {
    ::gdk_font_unref(mGdkFont);
  }
  if (mUnscaledBitmap) {
    XFreePixmap(mDisplay, mUnscaledBitmap);
  }
}

//
// scale_image:
//
// Scale an image from a source area to a destination area
// using anti-aliasing.
//
// For performance reasons the scaling is done in 2 steps:
//   horizontal scaling
//   vertical scaling
//
// It is possible to do the scaling by expanding the souce
// into a a temporary buffer that is the Cartesian product of
// the source and destination. For a source image of Sw*Sh and a
// destination of Dw*Dh the temporary buffer is Tw=Sw*Dw wide and
// Th=Sh*Dh high; eg: for a 20x30 source expanding into a 30x45 it would
// use a temporary buffer of ((20*30) * (30*45)) bytes = 810k points.
// It takes (Sh*Dh)*(Sh*Dh) operation to expand into the the temp buffer
// and the same number to compress into the dest buffer for a total
// of 2*(Sh*Dh)*(Sh*Dh) operations (1.6 million for this example).
// 
// If the expansion/compression is first done horizontally and *summed*
// into a temporary buffer that is the width of the destination and
// the height of the source it takes (Sw*Dw)*Sh operations. To 
// expanded/compress the temp buffer into the destination takes
// Tw*(Th*Dh) = Dw*(Sh*Dh) operations. The total is now
// (Sw*Dw)*Sh + Dw*(Sh*Dh) (in this example (20*30)*30 + 30(30*45) =
// 18000 + 40500 = 58.5K operations or 3.7% of the Cartesian product).


//
//
static void
scale_image(nsAntiAliasedGlyph *aSrc, nsAntiAliasedGlyph *aDst)
{
  PRUint32 x, y, col;
  PRUint8 buffer[65536];
  PRUint8 *horizontally_scaled_data = buffer;
  PRUint8 *pHsd, *pDst;
  PRUint32 dst_width = aDst->GetWidth();
  PRUint32 dst_buffer_width = aDst->GetBufferWidth();
  PRUint32 dst_height = aDst->GetHeight();
  PRUint8 *dst = aDst->GetBuffer();

  if (aDst->GetBorder() != 0) {
    NS_ASSERTION(aDst->GetBorder()!=0,"border not supported");
    return;
  }

  PRUint32 ratio;

  PRUint8 *src = aSrc->GetBuffer();
  PRUint32 src_width = aSrc->GetWidth();
  NS_ASSERTION(src_width,"zero width glyph");
  if (src_width==0)
    return;

  PRUint32 src_height = aSrc->GetHeight();
  NS_ASSERTION(src_height,"zero height glyph");
  if (src_height==0)
    return;

  //
  // scale the data horizontally
  //

  // Calculate the ratio between the unscaled horizontal and
  // the scaled horizontal. Use interger multiplication 
  // using a 24.8 format fixed decimal format.
  ratio = (dst_width<<8)/src_width;

  PRUint32 hsd_len = dst_buffer_width * src_height;
  // use the stack buffer if possible
  if (hsd_len > sizeof(buffer)) {
    horizontally_scaled_data = (PRUint8*)nsMemory::Alloc(hsd_len);
    memset(horizontally_scaled_data, 0, hsd_len);
  }
  for (y=0; y<(dst_buffer_width*src_height); y++)
    horizontally_scaled_data[y] = 0;

  pHsd = horizontally_scaled_data;
  for (y=0; y<src_height; y++,pHsd+=dst_buffer_width) {
    for (x=0; x<src_width; x++) {
      // get the unscaled value
      PRUint8 src_val = src[x + (y*src_width)];
      if (!src_val)
        continue;
      // transfer the unscaled point's value to the scaled image putting
      // the correct percentage into the appropiate scaled locations
      PRUint32 area_begin = x * ratio; // starts here (24.8 format)
      PRUint32 area_end   = (x+1) * ratio; // ends here (24.8 format)
      PRUint32 end_pixel  = (area_end+255)&0xffffff00; // round to integer
      // Walk thru the scaled pixels putting in the appropiate amount.
      // col is in 24.8 format
      for (col=(area_begin&0xffffff00); col<end_pixel; col+=256) {
        // figure out how much of the unscaled pixel should go
        // in this scaled pixel
        PRUint32 this_begin = MAX(col,area_begin);
        PRUint32 this_end   = MIN((col+256), area_end);
        // add in the amount for this scaled pixel
        pHsd[col>>8] += (PRUint8)(((this_end-this_begin)*src_val)>>8);
      }
      DEBUG_DUMP((dump_byte_table(horizontally_scaled_data,
                                  dst_width, src_height)));
    }
  }

  //
  // Scale the data vertically
  //

  // Calculate the ratio between the unscaled vertical and
  // the scaled vertical. Use interger multiplication 
  // using a 24.8 format fixed decimal format.
  ratio = (dst_height<<8)/src_height;

  for (x=0; x<dst_width; x++) {
    pHsd = horizontally_scaled_data + x;
    pDst = dst + x;
    for (y=0; y<src_height; y++,pHsd+=dst_buffer_width) {
      // get the unscaled value
      PRUint8 src_val = *pHsd;
      if (src_val == 0)
        continue;
      // transfer the unscaled point's value to the scaled image putting
      // the correct percentage into the appropiate scaled locations
      PRUint32 area_begin = y * ratio; // starts here (24.8 format)
      PRUint32 area_end   = area_begin + ratio; // ends here (24.8 format)
      PRUint32 end_pixel  = (area_end+255)&0xffffff00; // round to integer
      PRUint32 c;
      PRUint32 col;
      // Walk thru the scaled pixels putting in the appropiate amount.
      // c is in 24.8 format
      for (c=(area_begin>>8)*dst_buffer_width,col=(area_begin&0xffffff00);
                col<end_pixel; c+=dst_buffer_width,col+=256) {
        // figure out how much of the unscaled pixel should go
        // in this scaled pixel
        PRUint32 this_begin = MAX(col,area_begin);
        PRUint32 this_end   = MIN((col+256), area_end);
        // add in the amount for this scaled pixel
        pDst[c] += (((this_end-this_begin)*src_val)>>8);
      }
      DEBUG_DUMP((dump_byte_table(dst, dst_width, dst_height)));
    }
  }
  if (horizontally_scaled_data != buffer)
    free(horizontally_scaled_data);
}

//
// scale_imageAntiJag:
//
// Enlarge (scale) an image from a source area to a destination area
// using anti-aliasing. To smooth the edges the inside corners are 
// fade-filled and the outside corners fade-cleared (anti jagged)
//
// anti-jagging example:
//
//   - - - -                     - - - -            
// |********|                   |  ***   |           
// |********|                   | ****** |           
// |********|                   |********|           
// |********|            ->     | *********          
// |******** - - - -            |   ********* - -    
//  - - - -|********|            - - -*********  |   
//         |********|                   ******** |   
//         |********|                   | ****** |   
//         |********|                   |  ****  |   
//          - - - -                      - - - -
//
// Only fill-in/clear-out one half of the corner. For 45 degree
// lines this takes off the corners that stick out and fills in
// the inside corners. This should fill and clear about the same
// amount of pixels so the image should retain approximately the
// correct visual weight.
//
// This helps *alot* with lines at 45 degree angles.
// This helps somewhat with lines at other angles.
//
// This fills in the corners of lines that cross making the
// image seem a bit heavier.
//
// To do the anti-jaggin the code needs to look a the pixels surrounding
// a pixel to see if the pixel is an inside corner or an outside
// corner. To avoid handling the pixels on the outer edge as special
// cases the image is padded.
//
//

static void
scale_imageAntiJag(nsAntiAliasedGlyph *aSrc, nsAntiAliasedGlyph *aDst)
{
  PRUint32 x, y, col;
  PRUint8 buffer[65536];
  PRUint8 *padded_src = aSrc->GetBuffer();
  PRUint8 exp_buffer[65536];
  PRUint8 *horizontally_scaled_data = buffer;
  PRUint8 *pHsd, *pDst;
  PRUint32 dst_width = aDst->GetWidth();
  PRUint32 dst_buffer_width = aDst->GetBufferWidth();
  PRUint32 dst_height = aDst->GetHeight();
  PRUint8 *dst = aDst->GetBuffer();

  if (aDst->GetBorder() != 0) {
    NS_ASSERTION(aDst->GetBorder()==0, "non zero dest border not supported");
    return;
  }
  PRUint32 expand = (((dst_width<<8)/aSrc->GetWidth())+255)>>8;

  PRUint32 src_width     = aSrc->GetWidth();
  PRUint32 src_height    = aSrc->GetHeight();
  PRUint32 border_width  = aSrc->GetBorder();
  PRUint32 padded_width  = aSrc->GetWidth()  + (2*border_width);
  PRUint32 padded_height = aSrc->GetHeight() + (2*border_width);

  //
  // Expand the data (anti-jagging comes later)
  //
  PRUint32 expanded_width  = padded_width  * expand;
  PRUint32 expanded_height = padded_height * expand;

  PRUint32 start_x = border_width * expand;
  PRUint32 start_y = border_width * expand;

  PRUint8 *expanded_data = exp_buffer;
  PRUint32 exp_len = expanded_width*expanded_height;
  if (exp_len > sizeof(exp_buffer))
    expanded_data = (PRUint8*)malloc(expanded_width*expanded_height);
  for (y=0; y<padded_height; y++) {
    for (int i=0; i<expand; i++) {
      for (x=0; x<padded_width; x++) {
        PRUint32 padded_index = x+(y*padded_width);
        PRUint32 exp_index = (x*expand) + ((i+(y*expand))*expanded_width);
        for (int j=0; j<expand; j++) {
          expanded_data[exp_index+j] = padded_src[padded_index];
        }
      }
    }
  }
  DEBUG_DUMP((dump_byte_table(expanded_data, expanded_width, expanded_height)));

// macro to access the surrounding pixels
//
// +-------+-------+-------+
// |  up   |       |  up   |
// | left  |  up   | right |
// |       |       |       |
// +-------+-------+-------+
// |       |       |       |
// | left  |  SRC  | right |
// |       |       |       |
// +-------+-------+-------+
// |       |       |       |
// | down  | down  | down  |
// | left  |       | right |
// +-------+-------+-------+
//
//
#define SRC_UP_LEFT(ps)    *(ps-padded_width-1)
#define SRC_UP(ps)         *(ps-padded_width)
#define SRC_UP_RIGHT(ps)   *(ps-padded_width+1)
#define SRC_LEFT(ps)       *(ps-1)
#define SRC(ps)            *(ps)
#define SRC_RIGHT(ps)      *(ps+1)
#define SRC_DOWN_LEFT(ps)  *(ps+padded_width-1)
#define SRC_DOWN(ps)       *(ps+padded_width)
#define SRC_DOWN_RIGHT(ps) *(ps+padded_width+1)
#define FILL_VALUE 255

  //
  // Anti-Jag by doing triangular fade-fills and fade clears.
  //
  // To do a triangular fill/clear the code needs to do a double loop: 
  //
  //    one to scan vertically
  //    one to scan horizontally the correct amount to make a triangle
  //
  // eg: 
  //
  //    for (i=0; i<jag_len; i++)
  //      for (j=0; j<jag_len-i; j++)
  //
  // The way the index is calculated determines whether the triangle is:
  // (pseudo code)
  //
  //      up-right: buf[ j - i*width]
  //       up-left: buf[-j - i*width]
  //    down-right: buf[ j + i*width]
  //     down-left: buf[-j + i*width]
  //
  // The value to fill-in/clear is related to the distance from the
  // origin of this corner. The correct value is the square root
  // of the sum of the squares. This code does a rough approximation
  // by adding the x and y offsets 
  //
  //    (i+j)
  //
  // This approximation can darken the fill fade by up to 41% but it
  // is much less CPU intensive and does not seem to affect the visual
  // quality.
  //
  // Normalize the value to the 0-1 range (24.8) by dividing by the
  // triangle's size:
  //
  // ((i+j)<<8)/jag_len
  //
  // Finally convert it to a (8 bit) value
  //
  // (((((i+j)<<8)/jag_len) * FILL_VALUE) >> 8)
  //
  // Note: this code only works for black (0) and white (255) values
  //
  DEBUG_DUMP((dump_byte_table(expanded_data, expanded_width,expanded_height)));
  PRUint8 *ps, *es;
  PRUint32 jag_len = (expand)/2; // fill/clear one half of the corner
  PRUint32 i, j;
  for (y=0; y<src_height; y++) {
    ps = padded_src + (border_width + (border_width+y)*padded_width);
    es = expanded_data
         + (border_width+((border_width+y)*expanded_width))*expand;
    for (x=0; x<src_width; x++,ps++,es+=expand) {
      //
      // Fill in the inside of corners
      //
      if (SRC(ps)==0) {
        jag_len = ((expand+1)/2);
        if ((SRC_RIGHT(ps)==255) && (SRC_DOWN(ps)==255)) {
          // triangular fade fill
          for (i=0; i<jag_len; i++)
            for (j=0; j<jag_len-i; j++)
              es[expand-1-j+((expand-1-i)*expanded_width)] =
                            255-(((((i+j)<<8)/jag_len)*FILL_VALUE)>>8);
        }
        if ((SRC_DOWN(ps)==255) && (SRC_LEFT(ps)==255)) {
          // triangular fade fill
          for (i=0; i<jag_len; i++)
            for (j=0; j<jag_len-i; j++)
              es[j+((expand-1-i)*expanded_width)] =
                            255-(((((i+j)<<8)/jag_len)*FILL_VALUE)>>8);
        }
        if ((SRC_LEFT(ps)==255) && (SRC_UP(ps)==255)) {
          // triangular fade fill
          for (i=0; i<jag_len; i++)
            for (j=0; j<jag_len-i; j++)
              es[j+(i*expanded_width)] =
                            255-(((((i+j)<<8)/jag_len)*FILL_VALUE)>>8);
        }
        if ((SRC_UP(ps)==255) && (SRC_RIGHT(ps)==255)) {
          // triangular fade fill
          for (i=0; i<jag_len; i++)
            for (j=0; j<jag_len-i; j++)
              es[expand-1-j+(i*expanded_width)] =
                            255-(((((i+j)<<8)/jag_len)*FILL_VALUE)>>8);
        }
      }
      //
      // Round off the outside of corners
      else {
        jag_len = ((expand+1)/2);
        if ((SRC_UP_LEFT(ps)==0) && (SRC_UP(ps)==0) && (SRC_LEFT(ps)==0)) {
          // triangular fade clear
          for (i=0; i<jag_len; i++)
            for (j=0; j<jag_len-i; j++)
              es[j+(i*expanded_width)] =
                            (((((i+j)<<8)/jag_len)*FILL_VALUE)>>8);
        }
        if ((SRC_UP(ps)==0) && (SRC_UP_RIGHT(ps)==0) && (SRC_RIGHT(ps)==0)) {
          // triangular fade clear
          for (i=0; i<jag_len; i++)
            for (j=0; j<jag_len-i; j++)
              es[expand-1-j+(i*expanded_width)] =
                            (((((i+j)<<8)/jag_len)*FILL_VALUE)>>8);
        }
        if ((SRC_LEFT(ps)==0) && (SRC_DOWN_LEFT(ps)==0) && (SRC_DOWN(ps)==0)) {
          // triangular fade clear
          for (i=0; i<jag_len; i++)
            for (j=0; j<jag_len-i; j++)
              es[j+((expand-1-i)*expanded_width)] =
                            (((((i+j)<<8)/jag_len)*FILL_VALUE)>>8);
        }
        if ((SRC_RIGHT(ps)==0) && (SRC_DOWN_RIGHT(ps)==0) && (SRC_DOWN(ps)==0)){
          // triangular fade clear
          for (i=0; i<jag_len; i++)
            for (j=0; j<jag_len-i; j++)
              es[(expand-1-j)+((expand-1-i)*expanded_width)] =
                            (((((i+j)<<8)/jag_len)*FILL_VALUE)>>8);
        }
      }
    }
  }
  DEBUG_DUMP((dump_byte_table(expanded_data, expanded_width,expanded_height)));

  //
  // scale the data horizontally
  //
  // note: size is +dst_buffer_width because the loop can go one pixel
  // (horizontal or vertical) past array (with a near 0 width area)
  PRUint32 ratio = ((dst_width<<8)/expand)/src_width;

  PRUint32 hsd_len = (dst_buffer_width+1) * expanded_height;
  if (hsd_len > sizeof(buffer)) {
    horizontally_scaled_data = (PRUint8*)nsMemory::Alloc(hsd_len);
    memset(horizontally_scaled_data, 0, hsd_len);
  }
  for (i=0; i<hsd_len; i++)
    horizontally_scaled_data[i] = 0;

  PRUint32 len_x   = src_width * expand;
  pHsd = horizontally_scaled_data;
  for (y=0; y<expanded_height; y++,pHsd+=dst_buffer_width) {
    for (x=0; x<len_x; x++) {
      PRUint32 exp_index = start_x + x + (y*expanded_width);
      PRUint8 src_val = expanded_data[exp_index];
      if (!src_val)
        continue;
      PRUint32 area_begin = x * ratio;
      PRUint32 area_end   = (x+1) * ratio;
      PRUint32 end_pixel   = (area_end+255)&0xffffff00;
      for (col=(area_begin&0xffffff00); col<end_pixel; col+=256) {
        PRUint32 this_begin = MAX(col,area_begin);
        PRUint32 this_end   = MIN((col+256), area_end);
        pHsd[col>>8] += (PRUint8)(((this_end-this_begin)*src_val)>>8);
        if ((&pHsd[col>>8]-horizontally_scaled_data) > (int)hsd_len) {
          NS_ASSERTION(0, "buffer too small");
          return;
        }
      }
    }

    DEBUG_DUMP((dump_byte_table(horizontally_scaled_data,
                dst_width, expanded_height)));
  }

  //
  // scale the data vertically
  //
  ratio = ((dst_height<<8)/expand)/src_height;
  PRUint32 len_y   = src_height * expand;
  for (x=0; x<dst_width; x++) {
    pHsd = horizontally_scaled_data + x + (start_y*dst_buffer_width);
    pDst = dst + x;
    for (y=0; y<len_y; y++,pHsd+=dst_buffer_width) {
      PRUint8 src_val = *pHsd;
      if (src_val == 0)
        continue;
      PRUint32 area_begin = y * ratio;
      PRUint32 area_end   = area_begin + ratio;
      PRUint32 end_pixel   = (area_end+255)&0xffffff00;
      PRUint32 c;
      PRUint32 col;
      for (c=(area_begin>>8)*dst_buffer_width,col=(area_begin&0xffffff00);
                  col<end_pixel; c+=dst_buffer_width,col+=256) {
        PRUint32 this_begin = MAX(col,area_begin);
        PRUint32 this_end   = MIN((col+256), area_end);
        PRUint32 val = (((this_end-this_begin)*src_val)>>8);
        pDst[c] += val;
      }
    }
    DEBUG_DUMP((dump_byte_table(dst, dst_width, dst_height)));
  }
  if (expanded_data != exp_buffer)
    free(expanded_data);
  if (horizontally_scaled_data != buffer)
    free(horizontally_scaled_data);
}

#ifdef DEBUG
void
nsXFontAAScaledBitmap::dump_XImage_blue_data(XImage *ximage)
{
  int x, y;
  int width  = ximage->width;
  int height = ximage->height;
  int pitch = ximage->bytes_per_line;
  int depth  = DefaultDepth(sDisplay, DefaultScreen(sDisplay));
  PRUint8 *lineStart = (PRUint8 *)ximage->data;
  printf("dump_byte_table: width=%d, height=%d\n", width, height);
  printf("    ");
  for (x=0; (x<width)&&(x<75); x++) {
    if ((x%10) == 0)
      printf("+ ");
    else
      printf("- ");
  }
  printf("\n");
  if ((depth == 16) || (depth == 15)) {
    short *data = (short *)ximage->data;
    for (y=0; y<height; y++) {
      printf("%2d: ", y);
      data = (short *)lineStart;
      for (x=0; (x<width)&&(x<75); x++, data++) {
        printf("%02x", *data & 0x1F);
      }
      printf("\n");
      lineStart += ximage->bytes_per_line;
    }
  }
  else if ((depth == 24) || (depth == 32)) {
    long *data = (long *)ximage->data;
    for (y=0; y<height; y++) {
      printf("%2d: ", y);
      for (x=0; (x<width)&&(x<75); x++) {
        printf("%02x", (short)(data[x+(y*pitch)] & 0xFF));
      }
      printf("\n");
    }
  }
  else {
    printf("depth %d not supported\n", DefaultDepth(sDisplay, DefaultScreen(sDisplay)));
  }
}

void
dump_byte_table(PRUint8 *table, int width, int height)
{
  int x, y;
  printf("dump_byte_table: width=%d, height=%d\n", width, height);
  printf("    ");
  for (x=0; x<width; x++) {
    if ((x%10) == 0)
      printf("+ ");
    else
      printf("- ");
  }
  printf("\n");
  for (y=0; y<height; y++) {
    printf("%2d: ", y);
    for (x=0; x<width; x++) {
      PRUint8 val = table[x+(y*width)];
      printf("%02x", val);
    }
    printf("\n");
  }
  printf("---\n");
}
#endif

static void
WeightTableInitLinearCorrection(PRUint8* aTable, PRUint8 aMinValue,
                                double aGain)
{
  // setup the wieghting table
  for (int i=0; i<256; i++) {
    int val = i;
    if (i>=aMinValue)
      val += (int)rint((double)(i-aMinValue)*aGain);
    val = MAX(0, val);
    val = MIN(val, 255);
    aTable[i] = (PRUint8)val;
  }
}

