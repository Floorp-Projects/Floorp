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

#ifndef nsTitleFrame_h___
#define nsTitleFrame_h___

#include "nsBoxFrame.h"

#define NS_TITLE_FRAME_CID \
{ 0x73801d40, 0x5a22, 0x12d2, { 0x80, 0x46, 0x0, 0x60, 0x2, 0x14, 0xa7, 0x90 } }

class nsTitleFrame : public nsBoxFrame {
public:

  nsTitleFrame(nsIPresShell* aShell);
  virtual ~nsTitleFrame();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // we are always a vertical box.
  virtual PRBool GetInitialOrientation(PRBool& aIsHorizontal) { aIsHorizontal = PR_FALSE; return PR_TRUE; }

  // never autostretch we align out children.
  virtual PRBool GetInitialAutoStretch(PRBool& aStretch) { aStretch = PR_FALSE; return PR_TRUE; }

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif
};


#endif // guard
