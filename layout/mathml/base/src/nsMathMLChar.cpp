/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla MathML Project.
 * 
 * The Initial Developer of the Original Code is The University Of 
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
 */

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsString.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsMathMLOperators.h"
#include "nsMathMLChar.h"

//
// nsMathMLChar implementation
//

// Table of extensible chars in a suitable format for algorithmic purpose
// XXX Error-prone stuff. See how it can be automated.

/* Structure of the table gMathMLCharGlyph[]:
   The table is divided into sections, each corresponding to a
   stretchy symbol. Each section itself is divided into two blocks
   that are separated with the special null code 0x0000.
   The first block consists of *any number* of different glyphs
   of *increasing* size. The second block consists of exactly
   *4* glyphs that can be used to build a larger size.

   Another separate table, gMathMLCharIndex[], is used to store 
   pointers to the beginning of each section.

   3 steps are needed to extend the table:
   ---------------------------------------
   1. Append your new section in gMathMLCharGlyph[].
   2. Append your new enum in nsMathMLCharEnum, and  kMathMLChar
   3. Append a new pointer in gMathMLCharIndex[], so that 
      gMathMLCharIndex[your_enum] points to the begining of
      your new section in gMathMLCharGlyph[]. That is, 
      your_section = gMathMLCharGlyph[gMathMLCharIndex[your_enum]]
 */ 

// XXX The current values in the table correspond to the Symbol font.
//     Other fonts will need their own customized tables.

// XXX The Stretching doesn't align the glyphs in italic style. Need to
//     force the style to normal before starting to stretch.

static PRUnichar gMathMLCharGlyph[] = {

  //------------
  //index = 0
  //Section for: left parenthesis
  //Block of different variants of *increasing* size...	
  0x0028, // LEFT PARENTHESIS	# parenleft
          // more here ...
  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0xF8EB, // LEFT PAREN TOP	# parenlefttp (CUS)
  0xF8EC, // LEFT PAREN EXTENDER	# parenleftex (CUS)
  0xF8ED, // LEFT PAREN BOTTOM	# parenleftbt (CUS)
  0xF8EC, // glue = LEFT PAREN EXTENDER	# parenleftex (CUS)

  //------------
  //index = 6 
  //Section for: right parenthesis
  //Block of different variants of *increasing* size...	
  0x0029, // RIGHT PARENTHESIS	# parenright
          // more here ...
  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0xF8F6, // RIGHT PAREN TOP	# parenrighttp (CUS)
  0xF8F7, // MIDDLE = RIGHT PAREN EXTENDER	# parenrightex (CUS)
  0xF8F8, // RIGHT PAREN BOTTOM	# parenrightbt (CUS)
  0xF8F7, // glue = RIGHT PAREN EXTENDER	# parenrightex (CUS)

  //------------
  //index = 12
  //Section for: integral
  //Block of different variants of *increasing* size...	
  0x222B, // INTEGRAL	# integral
          // more here ...
  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0x2320, // TOP HALF INTEGRAL	# integraltp
  0xF8F5, // INTEGRAL EXTENDER	# integralex (CUS)
  0x2321, // BOTTOM HALF INTEGRAL	# integralbt
  0xF8F5, // glue = INTEGRAL EXTENDER	# integralex (CUS)

  //------------
  //index = 18
  //Section for: left square bracket
  //Block of different variants of *increasing* size...	
  0x005B, // LEFT SQUARE BRACKET	# bracketleft

  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0xF8EE, // LEFT SQUARE BRACKET TOP	# bracketlefttp (CUS)
  0xF8EF, // LEFT SQUARE BRACKET EXTENDER	# bracketleftex (CUS)
  0xF8F0, // LEFT SQUARE BRACKET BOTTOM	# bracketleftbt (CUS)
  0xF8EF, // glue = LEFT SQUARE BRACKET EXTENDER	# bracketleftex (CUS)

  //------------
  //index = 24
  //Section for: right square bracket
  //Block of different variants of *increasing* size...	
  0x005D, // RIGHT SQUARE BRACKET	# bracketright
 
  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0xF8F9, // RIGHT SQUARE BRACKET TOP	# bracketrighttp (CUS)
  0xF8FA, // RIGHT SQUARE BRACKET EXTENDER	# bracketrightex (CUS)
  0xF8FB, // RIGHT SQUARE BRACKET BOTTOM	# bracketrightbt (CUS)0
  0xF8FA, // glue = RIGHT SQUARE BRACKET EXTENDER	# bracketrightex (CUS)

  //------------
  //index = 30
  //Section for: left curly bracket
  //Block of different variants of *increasing* size...	
  0x007B, // LEFT CURLY BRACKET	# braceleft

  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0xF8F1, // LEFT CURLY BRACKET TOP	# bracelefttp (CUS)
  0xF8F2, // LEFT CURLY BRACKET MID	# braceleftmid (CUS)
  0xF8F3, // LEFT CURLY BRACKET BOTTOM	# braceleftbt (CUS)
  0xF8F4, // glue = CURLY BRACKET EXTENDER	# braceex (CUS)

  //------------
  //index = 36
  //Section for: right square bracket
  //Block of different variants of *increasing* size...	
  0x007D, // RIGHT CURLY BRACKET	# braceright

  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0xF8FC, // RIGHT CURLY BRACKET TOP	# bracerighttp (CUS)
  0xF8FD, // RIGHT CURLY BRACKET MID	# bracerightmid (CUS)
  0xF8FE, // RIGHT CURLY BRACKET BOTTOM	# bracerightbt (CUS)
  0xF8F4, // glue = CURLY BRACKET EXTENDER	# braceex (CUS)

  //------------
  //index = 42
  //Section for: down arrow
  //Block of different variants of *increasing* size...	
  0x2193, // DOWNWARDS ARROW    # arrowdown

  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0xF8E6, // VERTICAL ARROW EXTENDER	# arrowvertex (CUS)
  0xF8E6, // VERTICAL ARROW EXTENDER	# arrowvertex (CUS)
  0x2193, // DOWNWARDS ARROW    # arrowdown
  0xF8E6, // glue = VERTICAL ARROW EXTENDER	# arrowvertex (CUS)

  //------------
  //index = 48
  //Section for: up arrow
  //Block of different variants of *increasing* size...	
  0x2191, // UPWARDS ARROW	# arrowup

  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0x2191, // UPWARDS ARROW	# arrowup
  0xF8E6, // VERTICAL ARROW EXTENDER	# arrowvertex (CUS)
  0xF8E6, // VERTICAL ARROW EXTENDER	# arrowvertex (CUS)
  0xF8E6, // glue = VERTICAL ARROW EXTENDER	# arrowvertex (CUS)

  //------------
  //index = 54
  //Section for: left arrow
  //Block of different variants of *increasing* size...	
  0x2190, // LEFTWARDS ARROW	# arrowleft

  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0x2190, // LEFTWARDS ARROW	# arrowleft
  0xF8E7, // HORIZONTAL ARROW EXTENDER	# arrowhorizex (CUS)
  0xF8E7, // HORIZONTAL ARROW EXTENDER	# arrowhorizex (CUS)
  0xF8E7, // glue = HORIZONTAL ARROW EXTENDER	# arrowhorizex (CUS)
 
  //------------
  //index = 60
  //Section for: right arrow
  //Block of different variants of *increasing* size...	
  0x2192, // RIGHTWARDS ARROW	# arrowright

  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0xF8E7, // HORIZONTAL ARROW EXTENDER	# arrowhorizex (CUS)
  0xF8E7, // HORIZONTAL ARROW EXTENDER	# arrowhorizex (CUS)
  0x2192, // RIGHTWARDS ARROW	# arrowright
  0xF8E7, // glue = HORIZONTAL ARROW EXTENDER	# arrowhorizex (CUS) 
  
  //------------
  //index = 66
  //Section for: square-root bar
  0xF8E5, // RADICAL EXTENDER	# radicalex (CUS)

  0x0000, // That's all folks ...
  //Block of partial glyphs in the order top/left, middle, bottom/right, glue
  0xF8E5, // RADICAL EXTENDER	# radicalex (CUS)
  0xF8E5, // RADICAL EXTENDER	# radicalex (CUS)
  0xF8E5, // RADICAL EXTENDER	# radicalex (CUS)
  0xF8E5, // RADICAL EXTENDER	# radicalex (CUS)

  //------------
  //index = 72
  //end of table ...
  0x0000, 
};

// Pointers to the beginning of each section
static PRInt32 gMathMLCharIndex[] = {
   0,  // eMathMLChar_LeftParenthesis
   6,  // eMathMLChar_RightParenthesis
  12,  // eMathMLChar_Integral
  18,  // eMathMLChar_LeftSquareBracket
  24,  // eMathMLChar_RightSquareBracket
  30,  // eMathMLChar_LeftCurlyBracket
  36,  // eMathMLChar_RightCurlyBracket
  42,  // eMathMLChar_DownArrow, 
  48,  // eMathMLChar_UpArrow, 
  54,  // eMathMLChar_LeftArrow, 
  60,  // eMathMLChar_RightArrow, 
  66,  // eMathMLChar_RadicalBar, 
  72,  // safeguard, *must* always point at the *end* of gMathMLCharGlyph[]  
};

// data to enable a clean architecture and extensibility

static const PRInt32 kMathMLChar[] = {
  eMathMLChar_LeftParenthesis  ,  '(',
  eMathMLChar_RightParenthesis ,  ')',
  eMathMLChar_Integral         , 0x222B,
  eMathMLChar_LeftSquareBracket,  '[',
  eMathMLChar_RightSquareBracket, ']',
  eMathMLChar_LeftCurlyBracket,   '{',
  eMathMLChar_RightCurlyBracket,  '}',
  eMathMLChar_DownArrow,         0x2193,
  eMathMLChar_UpArrow,           0x2191,
  eMathMLChar_LeftArrow,         0x2190,
  eMathMLChar_RightArrow,        0x2192,
  eMathMLChar_RadicalBar,        0xF8E5,
//  eMathMLChar_Radical,           0x221A,
};

//---------------------------------------------

// Helper method that detects a new enum and cache it for you.
// You do not have to call this method. SetData() will call it
// for you whenever mData changes.
void
nsMathMLChar::SetEnum() {
  mEnum = eMathMLChar_DONT_STRETCH;
  if (1 == mData.Length()) {
    PRUnichar ch = mData[0];
    PRInt32 count = sizeof(kMathMLChar) / sizeof(kMathMLChar[0]);
    for (PRInt32 i = 0; i < count; i += 2) {
      if (ch == kMathMLChar[i+1]) {
        mEnum = nsMathMLCharEnum(kMathMLChar[i]);
        break;
      }
    }
  }
}

/*
 The Stretch:
 @param aContainerSize - suggested size for the stretched char
 @param aDesiredStretchSize - IN/OUT parameter. On input
 our current size, on output the size after stretching.

 How it works?
 The Stretch() method first looks for a glyph of appropriate
 size; If a glyph is found, it is cached by this object 
 and its size is returned in aDesiredStretchSize. The cached
 glyph will then be used at the painting stage.

 If no glyph of appropriate size is found, aDesiredStretchSize
 is set to aContainerSize and the char is later built by 
 parts, at the painting stage, to fit the size.

 * When a glyph is found, its size can actually be slighthly
   larger or smaller than aContainerSize.

 * When built by parts, the char is stretched to *exactly* 
   fit aContainerSize. [Is this what we want? Can we leave
   the leading?]

 In either case, it is the responsibility of the caller
 to account for the spacing when setting aContainerSize, and
 to leave the extra spacing when placing the stretched char.
*/

//#define SHOW_BORDERS 1

// 
NS_IMETHODIMP
nsMathMLChar::Stretch(nsIPresContext&      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      nsIStyleContext*     aStyleContext,
                      nsStretchDirection   aStretchDirection,
                      nsCharMetrics&       aContainerSize,
                      nsCharMetrics&       aDesiredStretchSize)
{
  nsresult rv = NS_OK;

  // quick return if there is nothing special about this char
  if (eMathMLChar_DONT_STRETCH == mEnum) {
    return NS_OK;
  }

  // check the common situations where stretching is not actually needed
  // XXX need better criteria
  if (aStretchDirection == NS_STRETCH_DIRECTION_VERTICAL) {
    if (aContainerSize.height <= aDesiredStretchSize.height) {
      mEnum = eMathMLChar_DONT_STRETCH; // ensure that the char behaves like a normal char
      return NS_OK;
    }
  }
  else if (aStretchDirection == NS_STRETCH_DIRECTION_HORIZONTAL) {
    if (aContainerSize.width <= aDesiredStretchSize.width) {
      mEnum = eMathMLChar_DONT_STRETCH; // ensure that the char behaves like a normal char
      return NS_OK;
    }
  }

//  printf("Container height:%d  ascent:%d  descent:%d\n contains height:%d  ascent:%d  descent:%d\n",
//         aContainerSize.height, aContainerSize.ascent, aContainerSize.descent,
//         aDesiredStretchSize.height, aDesiredStretchSize.ascent, 
//         aDesiredStretchSize.descent);

  // XXX Note: there are other symbols that just need to slightly
  //           increase their size, like \Sum

  nsStyleFont font;
  nsStyleColor color;
  aStyleContext->GetStyle(eStyleStruct_Font, font);
  aStyleContext->GetStyle(eStyleStruct_Color, color);

  // Set color and font
  aRenderingContext.SetColor(color.mColor);
  aRenderingContext.SetFont(font.mFont);

  nscoord height = aDesiredStretchSize.height;
  nscoord width = aDesiredStretchSize.width;
  nscoord ascent = aDesiredStretchSize.ascent; 
  nscoord descent = aDesiredStretchSize.descent;
  
  PRInt32 index = gMathMLCharIndex[mEnum];
  PRInt32 limit = gMathMLCharIndex[mEnum+1]; // next char starts here...
  nscoord w, h;

  // try first to see if there is a glyph of appropriate size ...
  PRBool sizeOK = PR_FALSE; 
  PRUnichar ch = gMathMLCharGlyph[index++];
  nsBoundingMetrics bm;
  while (ch && index <= limit) {
    // printf("Checking size of:%c  index:%d\n", ch & 0x00FF, index);
    rv = aRenderingContext.GetBoundingMetrics(&ch, PRUint32(1), bm);
    if (NS_FAILED(rv)) { printf("GetBoundingMetrics failed for %04X:%c\n", ch, ch&0x00FF); /*getchar();*/ return rv; }
    h = bm.ascent - bm.descent;
    w = bm.width;
 
    // printf("height:%d  width:%d  ascent:%d  descent:%d\n", 
    //         height, bm.width, bm.ascent, bm.descent);

    // XXX temp hack -- need better criteria
    if ((aStretchDirection == NS_STRETCH_DIRECTION_VERTICAL && h > height) || 
        (aStretchDirection == NS_STRETCH_DIRECTION_HORIZONTAL && w > width)) {
      sizeOK = PR_TRUE;
      descent = -bm.descent; // flip the sign, as expected by Gecko
      ascent = bm.ascent;
      height = bm.ascent - bm.descent;
      width = bm.width;
      mGlyph = ch;
      break;
    }
    ch = gMathMLCharGlyph[index++];
  }

  if (!sizeOK) { // see if we can build by parts...
    mGlyph = 0;
    PRInt32 i;
    nscoord a, d;
    h = w = a = d = 0;
    float flex[3] = {0.7f, 0.3f, 0.7f}; // XXX hack!
    if (aStretchDirection == NS_STRETCH_DIRECTION_VERTICAL) {
      // default is to fill-up the area given to us
      width = aDesiredStretchSize.width;
      height = aContainerSize.height;
      ascent = aContainerSize.ascent;
      descent = aContainerSize.descent;

      for (i = 0; i < 4; i++) {
        ch = gMathMLCharGlyph[index+i];
        rv = aRenderingContext.GetBoundingMetrics(&ch, PRUint32(1), bm);
        if (NS_FAILED(rv)) { printf("GetBoundingMetrics failed for %04X:%c\n", ch, ch&0x00FF); /*getchar();*/ return rv; }
        if (w < bm.width) w = bm.width;
        if (i < 3) h += nscoord(flex[i]*(bm.ascent-bm.descent)); // sum heights of the parts...
      }
      if (h <= aContainerSize.height) { // can nicely fit in the available space...
        width = w;
      } else { // sum of parts doesn't fit in the space... will use a single glyph
        mEnum = eMathMLChar_DONT_STRETCH; // ensure that the char behaves like a normal char
        return NS_OK;
      }
    }
    else if (aStretchDirection == NS_STRETCH_DIRECTION_HORIZONTAL) {
      // default is to fill-up the area given to us
      width = aContainerSize.width;
      height = aDesiredStretchSize.height;
      ascent = aDesiredStretchSize.ascent;
      descent = aDesiredStretchSize.descent;

      for (i = 0; i < 4; i++) {
        ch = gMathMLCharGlyph[index+i];
        rv = aRenderingContext.GetBoundingMetrics(&ch, PRUint32(1), bm);
        if (NS_FAILED(rv)) { printf("GetBoundingMetrics failed for %04X:%c\n", ch, ch&0x00FF); /*getchar();*/ return rv; }
        if (a < bm.ascent) a = bm.ascent;
        if (d > bm.descent) d = bm.descent;
        if (i < 3) w += nscoord(flex[i]*(bm.rightBearing-bm.leftBearing)); // sum widths of the parts...
      }
      if (w <= aContainerSize.width) { // can nicely fit in the available space...
//        ascent = a;
//        height = a - d;
      } else { // sum of parts doesn't fit in the space... will use a single glyph
        mEnum = eMathMLChar_DONT_STRETCH; // ensure that the char behaves like a normal char
        return NS_OK;
      }
    }
  }

  // cache the stretch direction to be used later at the painting stage
  mDirection = aStretchDirection;

  aDesiredStretchSize.width = width;
  aDesiredStretchSize.height = height;
  aDesiredStretchSize.ascent = ascent;
  aDesiredStretchSize.descent = descent;

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLChar::Paint(nsIPresContext&      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    nsIStyleContext*     aStyleContext)
{
  nsresult rv = NS_OK;

  nsStyleFont font;
  nsStyleColor color;
  aStyleContext->GetStyle(eStyleStruct_Font, font);
  aStyleContext->GetStyle(eStyleStruct_Color, color);

  // Set color and font ...
  aRenderingContext.SetColor(color.mColor);
  aRenderingContext.SetFont(font.mFont);

  // quick return if there is nothing special about this char ...
  if (eMathMLChar_DONT_STRETCH == mEnum) {
    aRenderingContext.DrawString(mData.GetUnicode(), PRUint32(mData.Length()), mRect.x, mRect.y);
    return NS_OK;
  }

  if (0 < mGlyph) { // wow, there is a glyph of appropriate size!
    aRenderingContext.DrawString(&mGlyph, PRUint32(1), mRect.x, mRect.y);
  }
  else { // paint by parts
    // grab some metrics that will help to adjust the placements ...
    nscoord fontAscent, fontDescent;
    nsCOMPtr<nsIFontMetrics> fm;
    aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));
    fm->GetMaxAscent(fontAscent);
    fm->GetMaxDescent(fontDescent);
    // do the painting ...
    if (mDirection == NS_STRETCH_DIRECTION_VERTICAL)
      return PaintVertically(aPresContext, aRenderingContext, aStyleContext,
                             fontAscent, fontDescent, mEnum, mRect);
    else if (mDirection == NS_STRETCH_DIRECTION_HORIZONTAL)
      return PaintHorizontally(aPresContext, aRenderingContext, aStyleContext,
                               fontAscent, fontDescent, mEnum, mRect);
  }
  return NS_OK;
}

/* ================================================================================= 
  And now the helper routines that actually do the job of painting the char by parts
 */

// draw a glyph in a clipped area so that we don't have hairy chars pending outside
void
nsMathMLChar::DrawChar(nsIRenderingContext& aRenderingContext, 
                       PRUnichar            aChar, 
                       nscoord              aX,
                       nscoord              aY,
                       nsRect&              aClipRect) 
{
  PRBool clipState;
  aRenderingContext.PushState();
  aRenderingContext.SetClipRect(aClipRect, nsClipCombine_kIntersect, clipState);
  aRenderingContext.DrawString(&aChar, PRUint32(1), aX, aY);
  aRenderingContext.PopState(clipState);
}

// paint a stretchy char by assembling glyphs vertically
nsresult
nsMathMLChar::PaintVertically(nsIPresContext&      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nsIStyleContext*     aStyleContext,
                              nscoord              fontAscent,
                              nscoord              fontDescent,
                              nsMathMLCharEnum     aCharEnum,
                              nsRect               aRect)
{
  nsresult rv = NS_OK;
  PRInt32 index = gMathMLCharIndex[aCharEnum];
  PRInt32 limit = gMathMLCharIndex[aCharEnum+1]; // next char starts here...
  // jump past the glyphs of various size...
  PRUnichar ch = gMathMLCharGlyph[index++];
  while (ch && index <= limit) {
   ch = gMathMLCharGlyph[index++];
  }
  // return if there are no partial glyphs (an erroneous table!)
  if (!gMathMLCharGlyph[index]) return NS_OK;

  nscoord dx = aRect.x;
  nscoord dy = aRect.y;
  nsRect clipRect;

  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t); 

  // get metrics data to be re-used later
  PRInt32 i;
  nsBoundingMetrics bm, bmdata[4];
  nscoord stride, offset[3], start[3], end[3];
  for (i = 0; i < 4; i++) {
    ch = gMathMLCharGlyph[index+i];
    rv = aRenderingContext.GetBoundingMetrics(&ch, PRUint32(1), bm);
    if (NS_FAILED(rv)) { printf("GetBoundingMetrics failed for %04X:%c\n", ch, ch&0x00FF); /*getchar();*/ return rv; }
    bmdata[i] = bm;
//    printf("bm for i:%d ch:%04X ascent:%d descent:%d lbearing:%d rbearing:%d width:%d fontAscent:%d fontDescent:%d\n",
//           i, ch, bm.ascent, bm.descent, bm.leftBearing, bm.rightBearing, bm.width, fontAscent, fontDescent);
  }
  for (i = 0; i < 3; i++) {
    ch = gMathMLCharGlyph[index+i];
    bm = bmdata[i];
//printf("Drawing i=%d ch=%04X", i, ch); getchar();
    if (0 == i) { // top
      //coord with line-spacing (i.e., leading)
      //  dy = aRect.y;

      //coord for exact fit (i.e., no leading)
      dy = aRect.y + bm.ascent - fontAscent;
    }
    else if (1 == i) { // middle
      // coord with line-spacing (i.e., leading)
      //  dy = aRect.y + (aRect.height - fontAscent - fontDescent
      //  - bmdata[0].ascent - bmdata[2].descent + bm.ascent + bm.descent)/2;

      // coord for exact fit (i.e., no leading)
      dy = aRect.y + (aRect.height - (bm.ascent - bm.descent))/2
                   - (fontAscent - bm.ascent);
    }
    else if (2 == i) { // bottom
      // coord with line-spacing (i.e., leading)
      // dy = aRect.y + aRect.height - (fontAscent + fontDescent);

      // coord for exact fit (i.e., no leading)
      dy = aRect.y + aRect.height - (fontAscent - bm.descent);
    }
    clipRect = nsRect(dx, dy, aRect.width, aRect.height);
    // abcissa that DrawString used
    offset[i] = dy;
    // *exact* abcissa where the *top-most* pixel of the glyph is painted
    start[i] = dy + fontAscent - bm.ascent; 
    // *exact* abcissa where the *bottom-most* pixel of the glyph is painted
    end[i] = dy + fontAscent - bm.descent; // note: end = start + height
  }

  /////////////////////////////////////
  // draw top, middle, bottom
  for (i = 0; i < 3; i++) {
    ch = gMathMLCharGlyph[index+i];
#ifdef SHOW_BORDERS
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    aRenderingContext.DrawRect(nsRect(dx,start[i],aRect.width+30*(i+1),end[i]-start[i]));
#endif
    dy = offset[i];
         if (i==0) clipRect = nsRect(dx, dy, aRect.width, aRect.height);
    else if (i==1) clipRect = nsRect(dx, end[0], aRect.width, start[2]-end[0]);
    else if (i==2) clipRect = nsRect(dx, start[2], aRect.width, end[2]-start[2]);

//    printf("Drawing i:%d ch:%04X ascent:%d descent:%d lbearing:%d rbearing:%d width:%d at position dx:%d dy:%d\n",
//           i, ch, bm.ascent, bm.descent, bm.leftBearing, bm.rightBearing, bm.width, dx, dy);
//    getchar();

    if (!clipRect.IsEmpty()) {
      clipRect.Inflate(onePixel, onePixel);
      DrawChar(aRenderingContext, ch, dx, dy, clipRect);
    }
  }

  ///////////////
  // fill the gap between top and middle, and between middle and bottom.
  ch = gMathMLCharGlyph[index+3]; // glue
  for (i = 0; i < 2; i++) {
    PRInt32 count = 0;
    dy = offset[i]; 
    clipRect = nsRect(dx, end[i], aRect.width, start[i+1]-end[i]);
    clipRect.Inflate(onePixel, onePixel);
#ifdef SHOW_BORDERS
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawRect(clipRect);
#endif
    bm = bmdata[i];
    while (dy + fontAscent - bm.descent < start[i+1]) {
      if (2 > count) {
        stride = -bm.descent;
        bm = bmdata[3]; // glue
        stride += bm.ascent;
      }
      count++;
      dy += stride;
      DrawChar(aRenderingContext, ch, dx, dy, clipRect);
//    NS_ASSERTION(5000 == count, "Error - gMathMLCharGlyph is incorrectly set");
      if (1000 == count) return NS_ERROR_UNEXPECTED;
    }
#ifdef SHOW_BORDERS
    // last glyph that may cross past its boundary and collide with the next
    nscoord height = bm.ascent - bm.descent;
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(nsRect(dx, dy+fontAscent-bm.ascent, aRect.width, height));
#endif
  }
  return NS_OK;	
}

// paint a stretchy char by assembling glyphs horizontally
nsresult
nsMathMLChar::PaintHorizontally(nsIPresContext&      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                nsIStyleContext*     aStyleContext,
                                nscoord              fontAscent,
                                nscoord              fontDescent,
                                nsMathMLCharEnum     aCharEnum,
                                nsRect               aRect)
{
  nsresult rv = NS_OK;
  PRInt32 index = gMathMLCharIndex[aCharEnum];
  PRInt32 limit = gMathMLCharIndex[aCharEnum+1]; // next char starts here...
  // jump past the glyphs of various size...
  PRUnichar ch = gMathMLCharGlyph[index++];
  while (ch && index <= limit) {
   ch = gMathMLCharGlyph[index++];
  }
  // return if there are no partial glyphs (an erroneous table!)
  if (!gMathMLCharGlyph[index]) return NS_OK;

  nscoord dx = aRect.x;
  nscoord dy = aRect.y;
  nsRect clipRect;

  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t); 

  // get metrics data to be re-used later
  PRInt32 i;
  nsBoundingMetrics bm, bmdata[4];
  nscoord stride, offset[3], start[3], end[3];
  for (i = 0; i < 4; i++) {
    ch = gMathMLCharGlyph[index+i];
    rv = aRenderingContext.GetBoundingMetrics(&ch, PRUint32(1), bm);
    if (NS_FAILED(rv)) { printf("GetBoundingMetrics failed for %04X:%c\n", ch, ch&0x00FF); /*getchar();*/ return rv; }
    bmdata[i] = bm;
//    printf("bm for i:%d ch:%04X ascent:%d descent:%d lbearing:%d rbearing:%d width:%d fontAscent:%d fontDescent:%d\n",
//           i, ch, bm.ascent, bm.descent, bm.leftBearing, bm.rightBearing, bm.width, fontAscent, fontDescent);
  }
  for (i = 0; i < 3; i++) {
    ch = gMathMLCharGlyph[index+i];
    bm = bmdata[i];
//printf("Drawing i=%d ch=%04X", i, ch); getchar();
    if (0 == i) { // left 
      // coord with intercharacter-spacing
      // dx = aRect.x;

      // coord for exact fit (no intercharacter-spacing)
      dx = aRect.x - bm.leftBearing;
    }
    else if (1 == i) { // middle
      // coord with intercharacter-spacing
      dx = aRect.x + (aRect.width - bm.width)/2;
    }
    else if (2 == i) { // right
      // coord with intercharacter-spacing
      // dx = aRect.x + aRect.width - bm.width;

      // coord for exact fit (no intercharacter-spacing)
      dx = aRect.x + aRect.width - bm.rightBearing;
    }
    // abcissa that DrawString used
    offset[i] = dx;
    // *exact* abcissa where the *left-most* pixel of the glyph is painted
    start[i] = dx + bm.leftBearing; 
    // *exact* abcissa where the *right-most* pixel of the glyph is painted
    end[i] = dx + bm.rightBearing; // note: end = start + width
  }

  ///////////////////////////
  // draw left, middle, right
  for (i = 0; i < 3; i++) {
    ch = gMathMLCharGlyph[index+i];
#ifdef SHOW_BORDERS
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    aRenderingContext.DrawRect(nsRect(start[i], dy, end[i]-start[i], aRect.height+30*(i+1)));
#endif
    dx = offset[i];
    nscoord top = PR_MIN(dy, dy + fontAscent - bm.ascent);
         if (i==0) clipRect = nsRect(dx, top, aRect.width, aRect.height);
    else if (i==1) clipRect = nsRect(end[0], top, start[2]-end[0], aRect.height);
    else if (i==2) clipRect = nsRect(start[2], top, end[2]-start[2], aRect.height);

//  printf("Drawing i:%d ch:%04X ascent:%d descent:%d lbearing:%d rbearing:%d width:%d at position dx:%d dy:%d\n",
//         i, ch, bm.ascent, bm.descent, bm.leftBearing, bm.rightBearing, bm.width, dx, dy);
//  getchar();
    if (!clipRect.IsEmpty()) {
      clipRect.Inflate(onePixel, onePixel);
      DrawChar(aRenderingContext, ch, dx, dy, clipRect);
    }
  }

  ////////////////
  // fill the gap between left and middle, and between middle and right.
  ch = gMathMLCharGlyph[index+3]; // glue
  for (i = 0; i < 2; i++) {
    PRInt32 count = 0;
    dx = offset[i]; 
    nscoord top = PR_MIN(dy, dy + fontAscent - bm.ascent);
    clipRect = nsRect(end[i], top, start[i+1]-end[i], aRect.height);
    clipRect.Inflate(onePixel, onePixel);
#ifdef SHOW_BORDERS
    // rectangles in-between that are to be filled
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawRect(clipRect);
#endif
    bm = bmdata[i];
    while (dx + bm.rightBearing < start[i+1]) {
      if (2 > count) {
        stride = bm.rightBearing;
        bm = bmdata[3]; // glue
        stride -= bm.leftBearing;
      }
      count++;
      dx += stride;
      DrawChar(aRenderingContext, ch, dx, dy, clipRect);
//    NS_ASSERTION(5000 == count, "Error - gMathMLCharGlyph is incorrectly set");
      if (1000 == count) return NS_ERROR_UNEXPECTED;
//    printf("Drawing %04X ascent:%d descent:%d  at position dx:%d dy:%d\n",
//           ch, bm.ascent, bm.descent, dx, dy);
    }
#ifdef SHOW_BORDERS
    // last glyph that may cross past its boundary and collide with the next
    nscoord width = bm.rightBearing - bm.leftBearing;
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(nsRect(dx + bm.leftBearing, dy, width, aRect.height));
#endif
  }
  return NS_OK;	
}

