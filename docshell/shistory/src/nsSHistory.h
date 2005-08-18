/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Radha Kulkarni <radha@netscape.com>
 */

#ifndef nsSHistory_h
#define nsSHistory_h

#include "nsCOMPtr.h"
#include "nsISHistory.h"

class nsISHEntry;
class nsISHTransaction;

class nsSHistory: public nsISHistory
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSISHISTORY

	nsSHistory();

protected:
	virtual ~nsSHistory();

private:
    friend NS_IMETHODIMP
		NS_NewSHistory(nsISupports * aOuter, REFNSIID aIID, void** aResult);
      NS_IMETHOD PrintHistory();
      NS_IMETHOD GetTransactionAtIndex(PRInt32 aIndex, nsISHTransaction ** aResult);
	nsCOMPtr<nsISHTransaction> mListRoot;
	PRInt32 mIndex;
	PRInt32 mLength;
};


#endif   /* nsSHistory */
