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

#include "nsMathMLAtoms.h"
#include "nsMathMLChar.h"
#include "nsMathMLOperators.h"

//
// nsMathMLChar implementation
//


// aDesiredStretchSize is an IN/OUT parameter.
NS_IMETHODIMP
nsMathMLChar::Stretch(nsIPresContext&    aPresContext,
                      nsIStyleContext*   aStyleContext,
                      nsStretchDirection aStretchDirection,
                      nsCharMetrics&     aContainerSize,
                      nsCharMetrics&     aDesiredStretchSize)
{
  if (aStretchDirection == NS_STRETCH_DIRECTION_VERTICAL) {
    // vertical stretching... for a boundary symbol
    aDesiredStretchSize.descent = aContainerSize.descent;
    aDesiredStretchSize.ascent  = aContainerSize.ascent;
    aDesiredStretchSize.height = aDesiredStretchSize.ascent 
                               + aDesiredStretchSize.descent;
  }
  else if (aStretchDirection == NS_STRETCH_DIRECTION_HORIZONTAL) {
    // horizontal stretching... for an accent
    aDesiredStretchSize.width = aContainerSize.width;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLChar::Paint(nsIPresContext&      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    nsIStyleContext*     aStyleContext,     
                    const nsPoint&       aOffset)
{
  nsresult rv = NS_OK;
  const nsStyleColor* color =
    (const nsStyleColor*)aStyleContext->GetStyleData(eStyleStruct_Color);
  const nsStyleFont* font =
    (const nsStyleFont*)aStyleContext->GetStyleData(eStyleStruct_Font);

  // Set color and font
  aRenderingContext.SetColor(color->mColor);
  aRenderingContext.SetFont(font->mFont);

  // Display the char 
  const PRUnichar* aString = mData.GetUnicode();  
  aRenderingContext.DrawString(aString, PRUint32(mData.Length()), aOffset.x, aOffset.y);

  return rv;
}

#if 0
// Each stretchy symbol can have its improved Stretch() and
// Paint() methods that override the default ones.

void
nsMathMLChar::StretchIntegral()
{
}

void
nsMathMLChar::PaintIntegral()
{
}

...

#endif


