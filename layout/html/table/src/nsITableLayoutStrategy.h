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

#ifndef nsITableLayoutStrategy_h__
#define nsITableLayoutStrategy_h__

#include "nscore.h"
#include "nsSize.h"

class nsIPresContext;
class nsIStyleContext;
struct nsReflowState;

class nsITableLayoutStrategy
{
public:

  /** assign widths for each column, taking into account the table content, the effective style, 
    * the layout constraints, and the compatibility mode.  Sets mColumnWidths as a side effect.
    * @param aPresContext     the presentation context
    * @param aTableStyle      the resolved style for the table
    * @param aMaxSize         the height and width constraints
    * @param aMaxElementSize  the min size of the largest indivisible object
    */
  virtual PRBool BalanceColumnWidths(nsIPresContext* aPresContext,
                                     nsIStyleContext *aTableStyle,
                                     const nsReflowState& aReflowState,
                                     PRInt32 maxWidth, 
                                     PRInt32 aNumCols,
                                     PRInt32 &aTotalFixedWidth,
                                     PRInt32 &aMinTableWidth,
                                     PRInt32 &aMaxTableWidth,
                                     nsSize* aMaxElementSize)=0;
};

#endif

