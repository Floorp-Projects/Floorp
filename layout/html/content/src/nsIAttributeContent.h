/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIAttributeContent_h___
#define nsIAttributeContent_h___

#include "nsISupports.h"

class nsIContent;
class nsIAtom;

// IID for the nsIAttributeContent class
#define NS_IATTRIBUTECONTENT_IID    \
{ 0xbed47b65, 0x54f9, 0x11d3, \
{ 0x96, 0xe9, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

class nsIAttributeContent : public nsISupports {
public:
  //friend nsresult NS_NewAttributeContent(nsIAttributeContent** aNewFrame);
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IATTRIBUTECONTENT_IID)

  NS_IMETHOD Init(nsIContent* aContent, PRInt32 aNameSpaceID, nsIAtom* aAttrName) = 0;

};                                                                 

#endif /* nsIAttributeContent_h___ */
