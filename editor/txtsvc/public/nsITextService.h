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

#ifndef nsITextService_h__
#define nsITextService_h__

#include "nsISupports.h"

class nsITextServicesDocument;

/*
TextService interface to outside world
*/

#define NS_ITEXTSERVICE_IID                    \
{ /* 019718E0-CDB5-11d2-8D3C-000000000000 */    \
0x019718e0, 0xcdb5, 0x11d2,                     \
{ 0x8d, 0x3c, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }


/**
 *
 */
class nsITextService  : public nsISupports{
public:

  static const nsIID& GetIID() { static nsIID iid = NS_ITEXTSERVICE_IID; return iid; }

  /**
   *
   */
  NS_IMETHOD Init(nsITextServicesDocument *aDoc) = 0;
  NS_IMETHOD Execute() = 0;
  NS_IMETHOD GetMenuString(nsString &aString) = 0;
};

#endif // nsITextService_h__

