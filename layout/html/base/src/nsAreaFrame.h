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
#ifndef nsAreaFrame_h___
#define nsAreaFrame_h___

#include "nsBlockFrame.h"
#include "nsVoidArray.h"
#include "nsAbsoluteContainingBlock.h"

struct nsStyleDisplay;
struct nsStylePosition;


/**
 * The area frame has an additional named child list:
 * - "Absolute-list" which contains the absolutely positioned frames
 *
 * @see nsLayoutAtoms::absoluteList
 */
class nsAreaFrame : public nsBlockFrame
{
public:
  friend nsresult NS_NewAreaFrame(nsIPresShell* aPresShell, nsIFrame** aResult, PRUint32 aFlags);
  
  // nsIFrame

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::areaFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

protected:
  nsAreaFrame();

};

#endif /* nsAreaFrame_h___ */
