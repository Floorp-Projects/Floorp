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
  friend nsresult NS_NewSelectsAreaFrame(nsIFrame** aResult, PRUint32 aFlags);

  // nsISupports
  //NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  
  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);

protected:
  PRBool IsOptionElement(nsIContent* aContent);
  PRBool IsOptionElementFrame(nsIFrame *aFrame);

};

#endif /* nsSelectsAreaFrame_h___ */
