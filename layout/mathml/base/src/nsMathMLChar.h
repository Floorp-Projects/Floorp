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

#ifndef nsMathMLChar_h___
#define nsMathMLChar_h___

#include "nsMathMLOperators.h"
#include "nsIMathMLFrame.h"

typedef PRUnichar nsGlyphCode;
class nsGlyphTable;

// List of enums of all known chars
enum nsMathMLCharEnum {
  eMathMLChar_DONT_STRETCH = -1,
#define WANT_CHAR_ENUM	
  #include "nsMathMLCharList.h"
#undef WANT_CHAR_ENUM
  eMathMLChar_COUNT
};

// Hints for Stretch() to indicate criteria for stretching
#define NS_STRETCH_SMALLER -1 // don't stretch more than requested size
#define NS_STRETCH_NORMAL   0 // try to stretch to requested size - DEFAULT
#define NS_STRETCH_LARGER   1 // try to stretch more than requested size

// class used to handle stretchy symbols (accent, delimiter and boundary symbols)
class nsMathMLChar
{
public:
  // constructor and destructor
  nsMathMLChar()
  {
    mStyleContext = nsnull;
  }

  ~nsMathMLChar() // not a virtual destructor: this class is not intended to be subclassed
  {
  }
 
  nsresult
  Paint(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        const nsRect&        aDirtyRect,
        nsFramePaintLayer    aWhichLayer,
        nsIFrame*            aForFrame);

  // This is the method called to ask the char to stretch itself.
  // aDesiredStretchSize is an IN/OUT parameter.
  // On input  - it contains our current size, or zero if current size is unknown
  // On output - the same size or the new size that the char wants.
  nsresult
  Stretch(nsIPresContext*      aPresContext,
          nsIRenderingContext& aRenderingContext,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          nsBoundingMetrics&   aDesiredStretchSize,
          PRInt32              aStretchHint = NS_STRETCH_NORMAL);

  // If you call SetData(), it will lookup the enum of the data
  // and set mEnum for you. If the data is an arbitrary string for 
  // which no enum is defined, mEnum is set to eMathMLChar_DONT_STRETCH
  // and the data is interpreted as a normal string.
  void
  SetData(nsIPresContext* aPresContext,
          nsString&       aData);

  void
  GetData(nsString& aData) {
    aData = mData;
  }

  // If you call SetEnum(), it will lookup the actual value of the data and
  // set it for you. All the enums listed above have their corresponding data.
  void
  SetEnum(nsIPresContext*  aPresContext,
          nsMathMLCharEnum aEnum);

  nsMathMLCharEnum
  GetEnum() {
    return mEnum;
  }

  PRInt32
  Length() {
    return mData.Length();
  }

  nsStretchDirection
  GetStretchDirection() {
    return mDirection;
  }

  // Sometimes we only want to pass the data to another routine,
  // this function helps to avoid copying
  const PRUnichar*
  GetUnicode() {
    return mData.GetUnicode();
  }

  void
  GetRect(nsRect& aRect) {
    aRect = mRect;
  }

  void
  SetRect(const nsRect& aRect) {
    mRect = aRect;
  }

  // Metrics that _exactly_ enclose the char. The char *must* have *already* 
  // being stretched before you can call the GetBoundingMetrics() method.
  // IMPORTANT: since chars have their own style contexts, and may be rendered
  // with glyphs that are not in the parent font, just calling the default
  // aRenderingContext.GetBoundingMetrics(aChar) can give incorrect results.
  void
  GetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics) {
    aBoundingMetrics = mBoundingMetrics;
  }

  // Hooks to access the extra leaf style contexts given to the MathMLChars.
  // They provide an interface to make them acessible to the Style System via
  // the Get/Set AdditionalStyleContext() APIs. Owners of MathMLChars
  // should honor these APIs.
  nsresult
  GetStyleContext(nsIStyleContext** aStyleContext) const;

  nsresult
  SetStyleContext(nsIStyleContext* aStyleContext);

private:
  nsString           mData;
  nsMathMLCharEnum   mEnum;
  nsStretchDirection mDirection;
  nsRect             mRect;
  nsBoundingMetrics  mBoundingMetrics;
  nsIStyleContext*   mStyleContext;
  nsGlyphTable*      mGlyphTable;
  nsGlyphCode        mGlyph;

  // helper methods
  static nsresult
  PaintVertically(nsIPresContext*      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  nscoord              aFontAscent,
                  nsIStyleContext*     aStyleContext,
                  nsGlyphTable*        aGlyphTable,
                  nsMathMLCharEnum     aCharEnum,
                  nsRect               aRect);

  static nsresult
  PaintHorizontally(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    nscoord              aFontAscent,
                    nsIStyleContext*     aStyleContext,
                    nsGlyphTable*        aGlyphTable,
                    nsMathMLCharEnum     aCharEnum,
                    nsRect               aRect);
};

#endif /* nsMathMLChar_h___ */
