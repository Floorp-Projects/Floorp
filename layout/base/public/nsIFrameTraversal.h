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
#ifndef NSIFRAMETRAVERSAL_H
#define NSIFRAMETRAVERSAL_H

#include "nsISupports.h"
#include "nsIEnumerator.h"
#include "nsIFrame.h"

enum nsTraversalType{LEAF, EXTENSIVE, FASTEST
#ifdef IBMBIDI // Simon
   , VISUAL
#endif
}; 

// {1691E1F3-EE41-11d4-9885-00C04FA0CF4B}
#define NS_IFRAMETRAVERSAL_IID \
{ 0x1691e1f3, 0xee41, 0x11d4, { 0x98, 0x85, 0x0, 0xc0, 0x4f, 0xa0, 0xcf, 0x4b } }

class nsIFrameTraversal : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFRAMETRAVERSAL_IID)

  NS_IMETHOD NewFrameTraversal(nsIBidirectionalEnumerator **aEnumerator,
                              PRUint32 aType,
                              nsIPresContext* aPresContext,
                              nsIFrame *aStart) = 0;
};


#endif //NSIFRAMETRAVERSAL_H
