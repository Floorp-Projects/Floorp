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

class nsMathMLChar
{
public:
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   nsIStyleContext*     aStyleContext,
                   const nsRect&        aRect);

  // This is the method called to ask the char to stretch itself.
  // aDesiredStretchSize is an IN/OUT parameter.
  // On input  - it contains our current size.
  // On output - the same size or the new size, including padding that we want.
  NS_IMETHOD Stretch(nsIPresContext&  aPresContext,
                     nsIStyleContext* aStyleContext,
                     nsCharMetrics&   aContainerSize,
                     nsCharMetrics&   aDesiredStretchSize);

  void Init(nsIFrame*                aFrame,
            const PRInt32            aLength,
            const nsString&          aData,
            const nsOperatorFlags    aFlags,
            const float              aLeftSpace,
            const float              aRightSpace,
            const nsStretchDirection aDirection)
  {
    mFrame      = aFrame;
    mLength     = aLength;
    mData       = aData;
    mFlags      = aFlags;
    mLeftSpace  = aLeftSpace;
    mRightSpace = aRightSpace;
    mDirection  = aDirection;
  }

  void SetLength(const PRInt32 aLength)
  {
    mLength = aLength;
  }

  PRInt32 GetLength()
  {
    return mLength;
  }

  // constructor and destructor
  nsMathMLChar()
  {
  }

  ~nsMathMLChar()
  {
  }

protected:
  PRInt32            mLength;
  nsString           mData;
  nsOperatorFlags    mFlags;
  float              mLeftSpace;
  float              mRightSpace;
  nsStretchDirection mDirection;
  nsPoint            mOffset;
  nsIFrame*          mFrame; // up-pointer to the frame that contains us.
};

#endif /* nsMathMLChar_h___ */
