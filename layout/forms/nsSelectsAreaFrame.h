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
#ifndef nsSelectsAreaFrame_h___
#define nsSelectsAreaFrame_h___

#include "nsAreaFrame.h"
class nsIContent;

/**
 * The area frame has an additional named child list:
 * - "Absolute-list" which contains the absolutely positioned frames
 *
 * @see nsLayoutAtoms::absoluteList
 */
class nsSelectsAreaFrame : public nsAreaFrame
{
public:
  friend nsresult NS_NewSelectsAreaFrame(nsIPresShell* aShell, nsIFrame** aResult, PRUint32 aFlags);

  // nsISupports
  //NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);

protected:
  PRBool IsOptionElement(nsIContent* aContent);
  PRBool IsOptionElementFrame(nsIFrame *aFrame);

};

#endif /* nsSelectsAreaFrame_h___ */
