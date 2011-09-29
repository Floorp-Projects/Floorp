/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 *   Frederic Wang <fred.wang@free.fr>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMathMLChar_h___
#define nsMathMLChar_h___

#include "nsMathMLOperators.h"
#include "nsMathMLFrame.h"

class nsGlyphTable;

// Hints for Stretch() to indicate criteria for stretching
enum {
  // Don't stretch
  NS_STRETCH_NONE     = 0x00,
  // Variable size stretches
  NS_STRETCH_VARIABLE_MASK = 0x0F,
  NS_STRETCH_NORMAL   = 0x01, // try to stretch to requested size
  NS_STRETCH_NEARER   = 0x02, // stretch very close to requested size
  NS_STRETCH_SMALLER  = 0x04, // don't stretch more than requested size
  NS_STRETCH_LARGER   = 0x08, // don't stretch less than requested size
  // A largeop in displaystyle
  NS_STRETCH_LARGEOP  = 0x10,
  NS_STRETCH_INTEGRAL  = 0x20,

  // Intended for internal use:
  // Find the widest metrics that might be returned from a vertical stretch
  NS_STRETCH_MAXWIDTH = 0x40
};

// A single glyph in our internal representation is characterized by a 'code@font' 
// pair. The 'code' is interpreted as a Unicode point or as the direct glyph index
// (depending on the type of nsGlyphTable where this comes from). The 'font' is a
// numeric identifier given to the font to which the glyph belongs.
struct nsGlyphCode {
  PRUnichar code[2]; 
  PRInt32   font;

  PRInt32 Length() { return (code[1] == PRUnichar('\0') ? 1 : 2); }
  bool Exists() const
  {
    return (code[0] != 0);
  }
  bool operator==(const nsGlyphCode& other) const
  {
    return (other.code[0] == code[0] && other.code[1] == code[1] && 
            other.font == font);
  }
  bool operator!=(const nsGlyphCode& other) const
  {
    return ! operator==(other);
  }
};

// Class used to handle stretchy symbols (accent, delimiter and boundary symbols).
// There are composite characters that need to be built recursively from other
// characters. Since these are rare we use a light-weight mechanism to handle
// them. Specifically, as need arises we append a singly-linked list of child
// chars with their mParent pointing to the first element in the list, except in
// the originating first element itself where it points to null. mSibling points
// to the next element in the list. Since the originating first element is the
// parent of the others, we call it the "root" char of the list. Testing !mParent
// tells whether you are that "root" during the recursion. The parent delegates
// most of the tasks to the children.
class nsMathMLChar
{
public:
  // constructor and destructor
  nsMathMLChar(nsMathMLChar* aParent = nsnull) {
    MOZ_COUNT_CTOR(nsMathMLChar);
    mStyleContext = nsnull;
    mSibling = nsnull;
    mParent = aParent;
    mUnscaledAscent = 0;
    mScaleX = mScaleY = 1.0;
    mDrawNormal = PR_TRUE;
  }

  ~nsMathMLChar() { // not a virtual destructor: this class is not intended to be subclassed
    MOZ_COUNT_DTOR(nsMathMLChar);
    // there is only one style context owned by the "root" char
    // and it may be used by child chars as well
    if (!mParent && mStyleContext) { // only the "root" need to release it
      mStyleContext->Release();
    }
    if (mSibling) {
      delete mSibling;
    }
  }

  nsresult
  Display(nsDisplayListBuilder*   aBuilder,
          nsIFrame*               aForFrame,
          const nsDisplayListSet& aLists,
          const nsRect*           aSelectedRect = nsnull);
          
  void PaintForeground(nsPresContext* aPresContext,
                       nsRenderingContext& aRenderingContext,
                       nsPoint aPt,
                       bool aIsSelected);

  // This is the method called to ask the char to stretch itself.
  // @param aContainerSize - IN - suggested size for the stretched char
  // @param aDesiredStretchSize - OUT - the size that the char wants
  nsresult
  Stretch(nsPresContext*           aPresContext,
          nsRenderingContext&     aRenderingContext,
          nsStretchDirection       aStretchDirection,
          const nsBoundingMetrics& aContainerSize,
          nsBoundingMetrics&       aDesiredStretchSize,
          PRUint32                 aStretchHint = NS_STRETCH_NORMAL);

  void
  SetData(nsPresContext* aPresContext,
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

  // Get the maximum width that the character might have after a vertical
  // Stretch().
  //
  // @param aStretchHint can be the value that will be passed to Stretch().
  // It is used to determine whether the operator is stretchy or a largeop.
  // @param aMaxSize is the value of the "maxsize" attribute.
  // @param aMaxSizeIsAbsolute indicates whether the aMaxSize is an absolute
  // value in app units (PR_TRUE) or a multiplier of the base size (PR_FALSE).
  nscoord
  GetMaxWidth(nsPresContext* aPresContext,
              nsRenderingContext& aRenderingContext,
              PRUint32 aStretchHint = NS_STRETCH_NORMAL,
              float aMaxSize = NS_MATHML_OPERATOR_SIZE_INFINITY,
              // Perhaps just nsOperatorFlags aFlags.
              // But need DisplayStyle for largeOp,
              // or remove the largeop bit from flags.
              bool aMaxSizeIsAbsolute = false);

  // Metrics that _exactly_ enclose the char. The char *must* have *already*
  // being stretched before you can call the GetBoundingMetrics() method.
  // IMPORTANT: since chars have their own style contexts, and may be rendered
  // with glyphs that are not in the parent font, just calling the default
  // aRenderingContext.GetBoundingMetrics(aChar) can give incorrect results.
  void
  GetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics) {
    aBoundingMetrics = mBoundingMetrics;
  }

  void
  SetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics) {
    mBoundingMetrics = aBoundingMetrics;
  }

  // Hooks to access the extra leaf style contexts given to the MathMLChars.
  // They provide an interface to make them accessible to the Style System via
  // the Get/Set AdditionalStyleContext() APIs. Owners of MathMLChars
  // should honor these APIs.
  nsStyleContext* GetStyleContext() const;

  void SetStyleContext(nsStyleContext* aStyleContext);

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
  nsStyleContext*    mStyleContext;
  nsGlyphTable*      mGlyphTable;
  nsGlyphCode        mGlyph;
  // mFamily is non-empty when the family for the current size is different
  // from the family in the nsStyleContext.
  nsString           mFamily;
  // mUnscaledAscent is the actual ascent of the char.
  nscoord            mUnscaledAscent;
  // mScaleX, mScaleY are the factors by which we scale the char.
  float              mScaleX, mScaleY;
  // mDrawNormal indicates whether we use special glyphs or not.
  bool               mDrawNormal;

  class StretchEnumContext;
  friend class StretchEnumContext;

  // helper methods
  nsresult
  StretchInternal(nsPresContext*           aPresContext,
                  nsRenderingContext&     aRenderingContext,
                  nsStretchDirection&      aStretchDirection,
                  const nsBoundingMetrics& aContainerSize,
                  nsBoundingMetrics&       aDesiredStretchSize,
                  PRUint32                 aStretchHint,
                  float           aMaxSize = NS_MATHML_OPERATOR_SIZE_INFINITY,
                  bool            aMaxSizeIsAbsolute = false);

  nsresult
  ComposeChildren(nsPresContext*       aPresContext,
                  nsRenderingContext& aRenderingContext,
                  nsGlyphTable*        aGlyphTable,
                  nscoord              aTargetSize,
                  nsBoundingMetrics&   aCompositeSize,
                  PRUint32             aStretchHint);

  nsresult
  PaintVertically(nsPresContext*       aPresContext,
                  nsRenderingContext& aRenderingContext,
                  nsFont&              aFont,
                  nsStyleContext*      aStyleContext,
                  nsGlyphTable*        aGlyphTable,
                  nsRect&              aRect);

  nsresult
  PaintHorizontally(nsPresContext*       aPresContext,
                    nsRenderingContext& aRenderingContext,
                    nsFont&              aFont,
                    nsStyleContext*      aStyleContext,
                    nsGlyphTable*        aGlyphTable,
                    nsRect&              aRect);

  void
  ApplyTransforms(nsRenderingContext& aRenderingContext, nsRect &r);
};

#endif /* nsMathMLChar_h___ */
