/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsAbAddressCollecter_H_
#define _nsAbAddressCollecter_H_

#include "nsIAbAddressCollecter.h"
#include "nsCOMPtr.h"
#include "nsIAbAddressCollecter.h"
#include "nsIAddrDatabase.h"
#include "nsAddrDatabase.h"

class nsIPref;

class nsAbAddressCollecter : public nsIAbAddressCollecter
{
public:
	nsAbAddressCollecter();
	virtual ~nsAbAddressCollecter();

	NS_DECL_ISUPPORTS
  NS_DECL_NSIABADDRESSCOLLECTER

	nsresult OpenHistoryAB(nsIAddrDatabase **aDatabase);
	nsresult IsDomainExcluded(const char *address, nsIPref *pPref, PRBool *bExclude);
	nsresult SetNamesForCard(nsIAbCard *senderCard, const char *fullName);
	nsresult SplitFullName (const char *fullName, char **firstName, char **lastName);
private:
	static int PR_CALLBACK collectEmailAddressPrefChanged(const char *newpref, void *data);
	static int PR_CALLBACK collectEmailAddressEnableSizeLimitPrefChanged(const char *newpref, void *data);
	static int PR_CALLBACK collectEmailAddressSizeLimitPrefChanged(const char *newpref, void *data);
	void setupPrefs(void);
protected:
	nsCOMPtr <nsIAddrDatabase> m_historyAB;
	nsCOMPtr <nsIAbDirectory> m_historyDirectory;
	PRInt32 maxCABsize;
	PRBool collectAddresses;
	PRInt32 sizeLimitEnabled;

};

#endif  // _nsAbAddressCollecter_H_

