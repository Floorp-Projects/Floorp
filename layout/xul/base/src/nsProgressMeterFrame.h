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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/**

  David Hyatt & Eric D Vaughan.

  An XBL-based progress meter. 

  Attributes:

  value: A number between 0% and 100%
  align: horizontal or vertical
  mode: determined, undetermined (one shows progress other shows animated candy cane)

**/

#include "nsBoxFrame.h"

class nsIPresContext;
class nsIStyleContext;

class nsProgressMeterFrame : public nsBoxFrame
{
public:
  friend nsresult NS_NewProgressMeterFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aChildList);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

protected:
  nsProgressMeterFrame(nsIPresShell* aPresShell);
  virtual ~nsProgressMeterFrame();

private:
}; // class nsProgressMeterFrame
