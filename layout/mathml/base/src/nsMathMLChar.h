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
#include "nsIMathMLFrame.h"

// enum of chars that we know something about
enum nsMathMLCharEnum {
  eMathMLChar_DONT_STRETCH = -1,
#define MATHML_CHAR(_name, _value, _direction) eMathMLChar_##_name,
#include "nsMathMLCharList.h"
#undef MATHML_CHAR
  eMathMLChar_COUNT
};


// class used to handle stretchy symbols (accent and boundary symbol)
class nsMathMLChar
{
public:
  // constructor and destructor
  nsMathMLChar()
  {
  }

  virtual ~nsMathMLChar()
  {
  }
 
  NS_IMETHOD
  Paint(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        nsIStyleContext*     aStyleContext);

  // This is the method called to ask the char to stretch itself.
  // aDesiredStretchSize is an IN/OUT parameter.
  // On input  - it contains our current size.
  // On output - the same size or the new size that the char wants.
  NS_IMETHOD
  Stretch(nsIPresContext*      aPresContext,
          nsIRenderingContext& aRenderingContext,
          nsIStyleContext*     aStyleContext,
          nsStretchDirection   aStretchDirection,
          nsStretchMetrics&    aContainerSize,
          nsStretchMetrics&    aDesiredStretchSize);

  // If you call SetData(), it will lookup the enum of the data
  // and set mEnum for you. If the data is an arbitrary string for 
  // which no enum is defined, mEnum is set to eMathMLChar_DONT_STRETCH
  // and the data is interpreted as a normal string.
  void
  SetData(nsString& aData);

  void
  GetData(nsString& aData) {
    aData = mData;
  }

  // If you call SetEnum(), it will lookup the actual value of the data and
  // set it for you. All the enums listed above have their corresponding data.
  void
  SetEnum(nsMathMLCharEnum aEnum);

  nsMathMLCharEnum
  Enum() {
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

  void
  GetBoundingMetrics(nsBoundingMetrics aBoundingMetrics) {
    aBoundingMetrics = mBoundingMetrics;
  }

private:
  nsString           mData;
  PRUnichar          mGlyph;
  nsRect             mRect;
  nsStretchDirection mDirection;
  nsMathMLCharEnum   mEnum;
  nsBoundingMetrics  mBoundingMetrics;

  // helper methods

  static void
  DrawGlyph(nsIRenderingContext& aRenderingContext, 
            PRUnichar            aChar, 
            nscoord              aX,
            nscoord              aY,
            nsRect&              aClipRect);

  static nsresult
  PaintVertically(nsIPresContext*      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  nsIStyleContext*     aStyleContext,
                  nscoord              fontAscent,
                  nscoord              fontDescent,
                  nsMathMLCharEnum     aCharEnum,
                  nsRect               aRect);

  static nsresult
  PaintHorizontally(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    nsIStyleContext*     aStyleContext,
                    nscoord              fontAscent,
                    nscoord              fontDescent,
                    nsMathMLCharEnum     aCharEnum,
                    nsRect               aRect);
};

#endif /* nsMathMLChar_h___ */
