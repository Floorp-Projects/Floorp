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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*used to pass principals through xpcom in arrays*/
#ifndef _NS_PRINCIPAL_ARRAY_H_
#define _NS_PRINCIPAL_ARRAY_H_

#include "nsIPrincipalArray.h"
#include "nsIPrincipal.h"
#include "nsVector.h"
#include "nsHashtable.h"

#define NS_PRINCIPALARRAY_CID \
{ 0x7ff2a4c0, 0x4bff, 0x17d3, \
{ 0xba, 0x18, 0x42, 0x60, 0xbb, 0xf1, 0x99, 0xa2 }}
NS_DEFINE_STATIC_CID_ACCESSOR(NS_PRINCIPALARRAY_CID)

class nsPrincipalArray : public nsIPrincipalArray {
public:

NS_DECL_ISUPPORTS
NS_DECL_NSIPRINCIPALARRAY

nsPrincipalArray(void);
nsPrincipalArray(PRUint32 count);

void
Init(PRUint32 count);

virtual ~nsPrincipalArray();

private:
nsVector * itsArray;
};

class PrincipalKey: public nsHashKey {
public:
	nsIPrincipal * itsPrincipal;

	PrincipalKey(nsIPrincipal * prin) {
		itsPrincipal = prin;
	}

	PRUint32 HashValue(void) const {
		PRUint32 * code = 0;
		itsPrincipal->HashCode(code);
		return *code;
	}

	PRBool Equals(const nsHashKey * aKey) const {
		PRBool result = PR_FALSE;
		itsPrincipal->Equals(((const PrincipalKey *) aKey)->itsPrincipal,& result);
		return result;
	}

	nsHashKey * Clone(void) const {
		return new PrincipalKey(itsPrincipal);
	}
};

#endif /* _NS_PRINCIPAL_ARRAY_H_ */
