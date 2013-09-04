/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSHTransaction_h
#define nsSHTransaction_h

// Helper Classes
#include "nsCOMPtr.h"

// Needed interfaces
#include "nsISHTransaction.h"

class nsISHEntry;

class nsSHTransaction: public nsISHTransaction
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSISHTRANSACTION

	nsSHTransaction();

protected:
	virtual ~nsSHTransaction();


protected:
   bool           mPersist;

	nsISHTransaction * mPrev; // Weak Reference
	nsCOMPtr<nsISHTransaction> mNext;
	nsCOMPtr<nsISHEntry>  mSHEntry;
};


#endif   /* nsSHTransaction_h */
