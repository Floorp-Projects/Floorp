/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIEntityConverter.h"
#include "nsIFactory.h"
#include "nsIProperties.h"

class nsEntityEntry
{
public:
	PRUnichar	mChar;
	PRUnichar*	mEntityValue;
};

class nsEntityConverter: public nsIEntityConverter
{
public:
	
	//
	// implementation methods
	//
	nsEntityConverter();
	virtual ~nsEntityConverter();

	//
	// nsISupports
	//
	NS_DECL_ISUPPORTS

	//
	// nsIEntityConverter
	//
	NS_IMETHOD ConvertToEntity(PRUnichar character, PRUnichar **_retval);

protected:

	void LoadEntityProperties(nsIPersistentProperties* entityProperties);
	nsString		mEntityListName;
	nsEntityEntry	*mEntityList;
	PRInt32			mEntityListLength;

};


class nsEntityConverterFactory: public nsIFactory
{
public:

	//
	// implementation methods
	//
	nsEntityConverterFactory();
	virtual ~nsEntityConverterFactory();

	//
	// nsISupports methods
	//
	NS_DECL_ISUPPORTS

	//
	// nsIFactory methods
	//
	NS_IMETHOD CreateInstance(nsISupports* aOuter,REFNSIID aIID,void** aResult);
	NS_IMETHOD LockFactory(PRBool aLock);
};

