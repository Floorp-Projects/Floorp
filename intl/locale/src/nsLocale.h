/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */
#ifndef nsLocale_h__
#define nsLocale_h__

#include "nsStringFwd.h"
#include "nsILocale.h"
#include "plhash.h"

class nsLocale : public nsILocale {
	friend class nsLocaleService;
	NS_DECL_THREADSAFE_ISUPPORTS

public:
	nsLocale(void);
	virtual ~nsLocale(void);
	
	/* Declare methods from nsILocale */
	NS_DECL_NSILOCALE

protected:
	
	NS_IMETHOD AddCategory(const nsAString& category, const nsAString& value);

	static PLHashNumber Hash_HashFunction(const void* key);
	static int Hash_CompareNSString(const void* s1, const void* s2);
	static int Hash_EnumerateDelete(PLHashEntry *he, int hashIndex, void *arg);

	PLHashTable*	fHashtable;
	uint32_t		fCategoryCount;

};


#endif
