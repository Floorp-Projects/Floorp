/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsStyleUtil_h___
#define nsStyleUtil_h___

#include "nslayout.h"
#include "nsCoord.h"

class nsIStyleContext;
struct nsStyleColor;

// Style utility functions
class nsStyleUtil {
public:
  
  static float GetScalingFactor(PRInt32 aScaler);

  static nscoord CalcFontPointSize(PRInt32 aHTMLSize, PRInt32 aBasePointSize, 
                                   float aScalingFactor);
  static PRInt32 FindNextSmallerFontSize(nscoord aFontSize, PRInt32 aBasePointSize, 
                                         float aScalingFactor);
  static PRInt32 FindNextLargerFontSize(nscoord aFontSize, PRInt32 aBasePointSize, 
                                        float aScalingFactor);

  static PRInt32 ConstrainFontWeight(PRInt32 aWeight);

  static const nsStyleColor* FindNonTransparentBackground(nsIStyleContext* aContext);
};


#endif /* nsStyleUtil_h___ */
