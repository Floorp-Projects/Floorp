/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsStyleUtil_h___
#define nsStyleUtil_h___

#include "nslayout.h"
#include "nsCoord.h"
#include "nsIPresContext.h"

class nsIStyleContext;
struct nsStyleColor;

enum nsFontSizeType {
  eFontSize_HTML  	= 1,
  eFontSize_CSS			= 2
};


// Style utility functions
class nsStyleUtil {
public:
  
  static float GetScalingFactor(PRInt32 aScaler);

  static nscoord CalcFontPointSize(PRInt32 aHTMLSize, PRInt32 aBasePointSize, 
                                   float aScalingFactor, nsIPresContext* aPresContext,
                                   nsFontSizeType aFontSizeType = eFontSize_HTML);

  static PRInt32 FindNextSmallerFontSize(nscoord aFontSize, PRInt32 aBasePointSize, 
                                         float aScalingFactor, nsIPresContext* aPresContext,
                                         nsFontSizeType aFontSizeType = eFontSize_HTML);

  static PRInt32 FindNextLargerFontSize(nscoord aFontSize, PRInt32 aBasePointSize, 
                                        float aScalingFactor, nsIPresContext* aPresContext,
                                        nsFontSizeType aFontSizeType = eFontSize_HTML);

  static PRInt32 ConstrainFontWeight(PRInt32 aWeight);

  static const nsStyleColor* FindNonTransparentBackground(nsIStyleContext* aContext,
                                                          PRBool aStartAtParent = PR_FALSE);
};


#endif /* nsStyleUtil_h___ */
