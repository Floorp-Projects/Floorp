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

// Structure used for a char's size and alignment information.
struct nsCharMetrics {
//  nscoord leading;
  nscoord descent, ascent;
  nscoord width, height;

  nsCharMetrics(nscoord Descent=0, nscoord Ascent=0, 
                nscoord Width=0, nscoord Height=0) {
    width = Width; 
    height = Height;
    ascent = Ascent; 
    descent = Descent;
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
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   nsIStyleContext*     aStyleContext,
                   const nsPoint&       aOffset);

  // This is the method called to ask the char to stretch itself.
  // aDesiredStretchSize is an IN/OUT parameter.
  // On input  - it contains our current size.
  // On output - the same size or the new size that we want.
  NS_IMETHOD Stretch(nsIPresContext&    aPresContext,
                     nsIStyleContext*   aStyleContext,
                     nsStretchDirection aStretchDirection,
                     nsCharMetrics&     aContainerSize,
                     nsCharMetrics&     aDesiredStretchSize);

  void SetData(nsString& aData)
  {
    mData = aData;
  }

  void GetData(nsString& aData) {
    aData = mData;
  }

  // constructor and destructor
  nsMathMLChar()
  {
  }

  virtual ~nsMathMLChar()
  {
  }

protected:
  nsString  mData;
};

#endif /* nsMathMLChar_h___ */
