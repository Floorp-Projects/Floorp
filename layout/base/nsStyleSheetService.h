/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of interface for managing user and user-agent style sheets */

#ifndef nsStyleSheetService_h_
#define nsStyleSheetService_h_

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIMemoryReporter.h"
#include "nsIStyleSheetService.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

namespace mozilla {
class CSSStyleSheet;
}
class nsICategoryManager;
class nsIMemoryReporter;
class nsISimpleEnumerator;

#define NS_STYLESHEETSERVICE_CID \
{ 0x3b55e72e, 0xab7e, 0x431b, \
  { 0x89, 0xc0, 0x3b, 0x06, 0xa8, 0xb1, 0x40, 0x16 } }

#define NS_STYLESHEETSERVICE_CONTRACTID \
  "@mozilla.org/content/style-sheet-service;1"

class nsStyleSheetService final
  : public nsIStyleSheetService
  , public nsIMemoryReporter
{
 public:
  nsStyleSheetService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTYLESHEETSERVICE
  NS_DECL_NSIMEMORYREPORTER

  nsresult Init();

  nsTArray<RefPtr<mozilla::CSSStyleSheet>>* AgentStyleSheets()
  {
    return &mSheets[AGENT_SHEET];
  }
  nsTArray<RefPtr<mozilla::CSSStyleSheet>>* UserStyleSheets()
  {
    return &mSheets[USER_SHEET];
  }
  nsTArray<RefPtr<mozilla::CSSStyleSheet>>* AuthorStyleSheets()
  {
    return &mSheets[AUTHOR_SHEET];
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  static nsStyleSheetService *GetInstance();
  static nsStyleSheetService *gInstance;

 private:
  ~nsStyleSheetService();

  void RegisterFromEnumerator(nsICategoryManager  *aManager,
                                          const char          *aCategory,
                                          nsISimpleEnumerator *aEnumerator,
                                          uint32_t             aSheetType);

  int32_t FindSheetByURI(const nsTArray<RefPtr<mozilla::CSSStyleSheet>>& aSheets,
                         nsIURI* aSheetURI);

  // Like LoadAndRegisterSheet, but doesn't notify.  If successful, the
  // new sheet will be the last sheet in mSheets[aSheetType].
  nsresult LoadAndRegisterSheetInternal(nsIURI *aSheetURI,
                                        uint32_t aSheetType);

  nsTArray<RefPtr<mozilla::CSSStyleSheet>> mSheets[3];
};

#endif
