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

#ifndef nsWin32LocaleFactory_h__
#define nsWin32LocaleFactory_h__

#include "nsString.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIWin32Locale.h"
#include "nsIWin32LocaleImpl.h"


class nsIWin32LocaleFactory : public nsIFactory
{
  NS_DECL_ISUPPORTS

public:

   
	nsIWin32LocaleFactory(void);
	virtual ~nsIWin32LocaleFactory(void);
	
	NS_IMETHOD CreateInstance(nsISupports* aOuter, REFNSIID aIID,
		void** aResult);

	NS_IMETHOD LockFactory(PRBool aLock);


};

#endif /* nsWin32LocaleFactory_h__ */