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


#ifndef nsIAnonymousContent_h___
#define nsIAnonymousContent_h___

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"

class nsISupportsArray;
class nsIAtom;

#define NS_IANONYMOUS_CONTENT_IID { 0x41a69e00, 0x2d6d, 0x12d3, { 0xb0, 0x33, 0xa1, 0x38, 0x71, 0x39, 0x78, 0x7c } }


/**
 * If a node is anonymous. Then it should implement this interface.
 */
class nsIAnonymousContent : public nsISupports {
public:
     static const nsIID& GetIID() { static nsIID iid = NS_IANONYMOUS_CONTENT_IID; return iid; }
};

#endif

