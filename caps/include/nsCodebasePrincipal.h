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
/* describes principals by their orginating uris*/
#ifndef _NS_CODEBASE_PRINCIPAL_H_
#define _NS_CODEBASE_PRINCIPAL_H_

#include "nsIPrincipal.h"

class nsCodebasePrincipal : public nsICodebasePrincipal {
public:

	NS_DECL_ISUPPORTS

	NS_IMETHOD
	GetURL(char ** cburl);

	NS_IMETHOD 
	IsCodebaseExact(PRBool * result);

	NS_IMETHOD
	IsCodebaseRegex(PRBool * result);

	NS_IMETHOD
	GetType(PRInt16 * type);

	NS_IMETHOD
	IsSecure(PRBool * result);

	NS_IMETHOD
	ToString(char ** result);

	NS_IMETHOD
	HashCode(PRUint32 * code);

	NS_IMETHOD
	Equals(nsIPrincipal * other, PRBool * result);

	nsCodebasePrincipal(PRInt16 type, const char *codebaseURL);
	virtual ~nsCodebasePrincipal(void);

protected:
	const char * itsCodeBaseURL;
	PRInt16 itsType;
};

#endif // _NS_CODEBASE_PRINCIPAL_H_
