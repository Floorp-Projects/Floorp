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

#ifndef nsITableLayoutStrategy_h__
#define nsITableLayoutStrategy_h__

#include "nscore.h"
#include "nsSize.h"

class nsIPresContext;
class nsIStyleContext;
struct nsHTMLReflowState;
struct nsMargin;
class nsTableCellFrame;
class nsStyleCoord;
class nsISizeOfHandler;

class nsITableLayoutStrategy
{
public:
  virtual ~nsITableLayoutStrategy() {};

  /** call once every time any table thing changes (content, structure, or style) 
    * @param aPresContext - the presentation context
	   * @param aReflowState - the reflow state for mTableFrame
    */
  virtual PRBool Initialize(nsIPresContext*          aPresContext,
                            const nsHTMLReflowState& aReflowState)=0;

  /** assign widths for each column, taking into account the table content, the effective style, 
    * the layout constraints, and the compatibility mode.  Sets mColumnWidths as a side effect.
    * @param aPresContext - the presentation context
	   * @param aReflowState - the reflow state for mTableFrame
    */
  virtual PRBool BalanceColumnWidths(nsIPresContext*          aPresContext,
                                     const nsHTMLReflowState& aReflowState)=0;


  


#ifdef DEBUG
  virtual void SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const = 0;
#endif
};

#endif

