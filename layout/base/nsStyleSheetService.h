/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of interface for managing user and user-agent style sheets */

#ifndef nsStyleSheetService_h_
#define nsStyleSheetService_h_

#include "nsIStyleSheetService.h"
#include "nsCOMArray.h"
#include "nsIStyleSheet.h"
#include "mozilla/Attributes.h"

class nsISimpleEnumerator;
class nsICategoryManager;

#define NS_STYLESHEETSERVICE_CID \
{0xfcca6f83, 0x9f7d, 0x44e4, {0xa7, 0x4b, 0xb5, 0x94, 0x33, 0xe6, 0xc8, 0xc3}}

#define NS_STYLESHEETSERVICE_CONTRACTID \
  "@mozilla.org/content/style-sheet-service;1"

class nsStyleSheetService MOZ_FINAL : public nsIStyleSheetService
{
 public:
  nsStyleSheetService() NS_HIDDEN;
  ~nsStyleSheetService() NS_HIDDEN;

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTYLESHEETSERVICE

  NS_HIDDEN_(nsresult) Init();

  nsCOMArray<nsIStyleSheet>* AgentStyleSheets() { return &mSheets[AGENT_SHEET]; }
  nsCOMArray<nsIStyleSheet>* UserStyleSheets() { return &mSheets[USER_SHEET]; }

  static nsStyleSheetService *gInstance;

 private:

  NS_HIDDEN_(void) RegisterFromEnumerator(nsICategoryManager  *aManager,
                                          const char          *aCategory,
                                          nsISimpleEnumerator *aEnumerator,
                                          PRUint32             aSheetType);

  NS_HIDDEN_(PRInt32) FindSheetByURI(const nsCOMArray<nsIStyleSheet> &sheets,
                                     nsIURI *sheetURI);

  // Like LoadAndRegisterSheet, but doesn't notify.  If successful, the
  // new sheet will be the last sheet in mSheets[aSheetType].
  NS_HIDDEN_(nsresult) LoadAndRegisterSheetInternal(nsIURI *aSheetURI,
                                                    PRUint32 aSheetType);
  
  nsCOMArray<nsIStyleSheet> mSheets[2];
};

#endif
