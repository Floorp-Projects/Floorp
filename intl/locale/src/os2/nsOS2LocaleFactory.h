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
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 06/01/2000       IBM Corp.      created this file modeled after its unix counter part: nsPosixLocaleFactory.cpp.
 *
 */


#ifndef nsOS2LocaleFactory_h__
#define nsOS2LocaleFactory_h__

#include "nsString.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIOS2Locale.h"
#include "nsOS2Locale.h"


class nsOS2LocaleFactory : public nsIFactory 
{
  NS_DECL_ISUPPORTS

public:

   
	nsOS2LocaleFactory(void);
	virtual ~nsOS2LocaleFactory(void);
	
	NS_IMETHOD CreateInstance(nsISupports* aOuter, REFNSIID aIID,
		void** aResult);

	NS_IMETHOD LockFactory(PRBool aLock);


};

#endif /* nsOS2LocaleFactory_h__ */
