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
 */

#ifndef nsMathMLChar_h___
#define nsMathMLChar_h___

#include "nsMathMLOperators.h"

typedef PRUint32 nsStretchDirection;

#define NS_STRETCH_DIRECTION_HORIZONTAL   0
#define NS_STRETCH_DIRECTION_VERTICAL     1

// chars that we know how to stretch
enum nsMathMLCharEnum {
  eMathMLChar_UNKNOWN = -1,
  eMathMLChar_LeftParenthesis,
  eMathMLChar_RightParenthesis,
  eMathMLChar_Integral ,
  eMathMLChar_LeftSquareBracket,
  eMathMLChar_RightSquareBracket,
  eMathMLChar_LeftCurlyBracket, 
  eMathMLChar_RightCurlyBracket, 
  eMathMLChar_DownArrow, 
  eMathMLChar_UpArrow, 
  eMathMLChar_LeftArrow, 
  eMathMLChar_RightArrow,   
  eMathMLChar_RadicalBar,
  eMathMLChar_COUNT
};

// Structure used for a char's size and alignment information.
struct nsCharMetrics {
//  nscoord leading;
  nscoord descent, ascent;
  nscoord width, height;

  nsCharMetrics(nscoord aDescent=0, nscoord aAscent=0, 
                nscoord aWidth=0, nscoord aHeight=0) {
    width = aWidth; 
    height = aHeight;
    ascent = aAscent; 
    descent = aDescent;
  }

  nsCharMetrics(const nsCharMetrics& aCharMetrics) {
    width = aCharMetrics.width; 
    height = aCharMetrics.height;
    ascent = aCharMetrics.ascent; 
    descent = aCharMetrics.descent;
  }

  nsCharMetrics(const nsHTMLReflowMetrics& aReflowMetrics) {
    width = aReflowMetrics.width; 
    height = aReflowMetrics.height;
    ascent = aReflowMetrics.ascent; 
    descent = aReflowMetrics.descent;
  }

  void 
  operator=(const nsCharMetrics& aCharMetrics) {
    width = aCharMetrics.width; 
    height = aCharMetrics.height;
    ascent = aCharMetrics.ascent; 
    descent = aCharMetrics.descent;
  }

  PRBool
  operator==(const nsCharMetrics& aCharMetrics) {
    return (width == aCharMetrics.width &&
            height == aCharMetrics.height &&
            ascent == aCharMetrics.ascent &&
            descent == aCharMetrics.descent);
  }
};

// class used to handle stretchy symbols (accent and boundary symbols)
class nsMathMLChar
{
public:
  // constructor and destructor
  nsMathMLChar()
  {
  }

/*
  nsMathMLChar() : mData(),
                   mGlyph(0)
  {
    nsStr::Initialize(mData, eTwoByte); // with MathML, we are two-byte by default
  }
*/

  virtual ~nsMathMLChar()
  {
  }
 
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   nsIStyleContext*     aStyleContext);

  // This is the method called to ask the char to stretch itself.
  // aDesiredStretchSize is an IN/OUT parameter.
  // On input  - it contains our current size.
  // On output - the same size or the new size that we want.
  NS_IMETHOD Stretch(nsIPresContext&      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     nsIStyleContext*     aStyleContext,
                     nsStretchDirection   aStretchDirection,
                     nsCharMetrics&       aContainerSize,
                     nsCharMetrics&       aDesiredStretchSize);

  void SetData(nsString& aData)
  {
    mData = aData;
    SetEnum();
  }

  void GetData(nsString& aData) {
    aData = mData;
  }

  void
  GetRect(nsRect& aRect) {
    aRect = mRect;
  }

  void
  SetRect(const nsRect& aRect) {
    mRect = aRect;
  }

private:
  nsString         mData;
  PRUnichar        mGlyph;
  nsRect           mRect;
  PRInt32          mDirection;
  nsMathMLCharEnum mEnum;

  // helper methods
  void             
  SetEnum();

  static void
  DrawChar(nsIRenderingContext& aRenderingContext, 
           PRUnichar            aChar, 
           nscoord              aX,
           nscoord              aY,
           nsRect&              aClipRect);
};

#endif /* nsMathMLChar_h___ */
