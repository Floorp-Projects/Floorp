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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIMdbFactoryFactory_h__
#define nsIMdbFactoryFactory_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

class nsIMdbFactory;

// d5440650-2807-11d3-8d75-00805f8a6617
#define NS_IMDBFACTORYFACTORY_IID          \
{ 0xd5440650, 0x2807, 0x11d3, { 0x8d, 0x75, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0x17}}

// because Mork doesn't support XPCOM, we have to wrap the mdb factory interface
// with an interface that gives you an mdb factory.
class nsIMdbFactoryFactory : public nsISupports
{
public:
    static const nsIID& GetIID(void) { static nsIID iid = NS_IMDBFACTORYFACTORY_IID; return iid; }
	NS_IMETHOD GetMdbFactory(nsIMdbFactory **aFactory) = 0;
};

#endif
