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
#include "nsMathMLFrame.h"

class nsGlyphTable;

// Hints for Stretch() to indicate criteria for stretching
#define NS_STRETCH_NORMAL  0x00000001 // try to stretch to requested size - DEFAULT
#define NS_STRETCH_NEARER  0x00000002 // stretch very close to requested size
#define NS_STRETCH_SMALLER 0x00000004 // don't stretch more than requested size
#define NS_STRETCH_LARGER  0x00000008 // don't stretch less than requested size
#define NS_STRETCH_LARGEOP 0x00000010 // for a largeop in displaystyle

// A single glyph in our internal representation is characterized by a 'code@font' 
// pair. The 'code' is interpreted as a Unicode point or as the direct glyph index
// (depending on the type of nsGlyphTable where this comes from). The 'font' is a
// numeric identifier given to the font to which the glyph belongs.
struct nsGlyphCode {
  PRUnichar code; 
  PRInt32   font;

  // conversion operator to just return the code point that we generally want
  operator PRUnichar () {
    return code;
  }
};

// class used to handle stretchy symbols (accent, delimiter and boundary symbols)
class nsMathMLChar
{
public:
  // constructor and destructor
  nsMathMLChar(nsMathMLChar* aParent = nsnull) {
    MOZ_COUNT_CTOR(nsMathMLChar);
    mStyleContext = nsnull;
    mSibling = nsnull;
    mParent = aParent;
  }

  ~nsMathMLChar() { // not a virtual destructor: this class is not intended to be subclassed
    MOZ_COUNT_DTOR(nsMathMLChar);
    if (mSibling) {
      delete mSibling;
    }
  }

  nsresult
  Paint(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        const nsRect&        aDirtyRect,
        nsFramePaintLayer    aWhichLayer,
        nsIFrame*            aForFrame);

  // This is the method called to ask the char to stretch itself.
  // @param aContainerSize - IN - suggested size for the stretched char
  // @param aDesiredStretchSize - OUT - the size that the char wants
  nsresult
  Stretch(nsIPresContext*      aPresContext,
          nsIRenderingContext& aRenderingContext,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          nsBoundingMetrics&   aDesiredStretchSize,
          PRUint32             aStretchHint = NS_STRETCH_NORMAL);

  void
  SetData(nsIPresContext* aPresContext,
          nsString&       aData);

  void
  GetData(nsString& aData) {
    aData = mData;
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
  get() {
    return mData.get();
  }

  void
  GetRect(nsRect& aRect) {
    aRect = mRect;
  }

  void
  SetRect(const nsRect& aRect) {
    mRect = aRect;
    // shift the orgins of child chars if any 
    if (!mParent && mSibling) { // only a "root" having child chars can enter here
      for (nsMathMLChar* child = mSibling; child; child = child->mSibling) {
        nsRect rect; 
        child->GetRect(rect);
        rect.MoveBy(mRect.x, mRect.y);
        child->SetRect(rect);
      }
    }
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

protected:
  friend class nsGlyphTable;
  nsString           mData;

  // support for handling composite stretchy chars like TeX over/under braces
  nsMathMLChar*      mSibling;
  nsMathMLChar*      mParent;

private:
  nsRect             mRect;
  nsStretchDirection mDirection;
  nsBoundingMetrics  mBoundingMetrics;
  nsIStyleContext*   mStyleContext;
  nsGlyphTable*      mGlyphTable;
  nsGlyphCode        mGlyph;

  // helper methods
  nsresult
  ComposeChildren(nsIPresContext*      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  nsGlyphTable*        aGlyphTable,
                  nsBoundingMetrics&   aContainerSize,
                  nsBoundingMetrics&   aCompositeSize,
                  PRUint32             aStretchHint);

  static nsresult
  PaintVertically(nsIPresContext*      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  nsFont&              aFont,
                  nsIStyleContext*     aStyleContext,
                  nsGlyphTable*        aGlyphTable,
                  nsMathMLChar*        aChar,
                  nsRect&              aRect);

  static nsresult
  PaintHorizontally(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    nsFont&              aFont,
                    nsIStyleContext*     aStyleContext,
                    nsGlyphTable*        aGlyphTable,
                    nsMathMLChar*        aChar,
                    nsRect&              aRect);
};

#endif /* nsMathMLChar_h___ */
