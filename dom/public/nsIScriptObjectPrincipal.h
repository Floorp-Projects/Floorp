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

#ifndef nsIScriptObjectPrincipal_h__
#define nsIScriptObjectPrincipal_h__

#include "nsISupports.h"

class nsIPrincipal;


#define NS_ISCRIPTOBJECTPRINCIPAL_IID \
{ 0x98485f80, 0x9615, 0x11d2,  \
{ 0xbd, 0x92, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

/**
 * JS Object Principal information.
 */
class nsIScriptObjectPrincipal : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCRIPTOBJECTPRINCIPAL_IID)

  NS_IMETHOD       GetPrincipal(nsIPrincipal **aPrincipal) = 0;
};

#endif // nsIScriptObjectPrincipal_h__
