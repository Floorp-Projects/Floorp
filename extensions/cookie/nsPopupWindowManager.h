/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPopupWindowManager_h__
#define nsPopupWindowManager_h__

#include "nsCOMPtr.h"

#include "nsIObserver.h"
#include "nsIPermissionManager.h"
#include "nsIPopupWindowManager.h"
#include "nsWeakReference.h"

class nsIURI;

class nsPopupWindowManager : public nsIPopupWindowManager,
                             public nsIObserver,
                             public nsSupportsWeakReference {

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPOPUPWINDOWMANAGER
  NS_DECL_NSIOBSERVER

  nsPopupWindowManager();
  virtual ~nsPopupWindowManager();
  nsresult Init();

private:
  PRUint32                       mPolicy;
  nsCOMPtr<nsIPermissionManager> mPermissionManager;
};

// {822bcd11-6432-48be-9e9d-36f7804b7747}
#define NS_POPUPWINDOWMANAGER_CID \
 {0x822bcd11, 0x6432, 0x48be, {0x9e, 0x9d, 0x36, 0xf7, 0x80, 0x4b, 0x77, 0x47}}

#endif /* nsPopupWindowManager_h__ */
