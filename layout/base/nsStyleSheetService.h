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
#include "mozilla/Array.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StyleSheet.h"

class nsICategoryManager;
class nsIMemoryReporter;
class nsISimpleEnumerator;

namespace mozilla {
class PresShell;
}  // namespace mozilla

#define NS_STYLESHEETSERVICE_CID                     \
  {                                                  \
    0x3b55e72e, 0xab7e, 0x431b, {                    \
      0x89, 0xc0, 0x3b, 0x06, 0xa8, 0xb1, 0x40, 0x16 \
    }                                                \
  }

#define NS_STYLESHEETSERVICE_CONTRACTID \
  "@mozilla.org/content/style-sheet-service;1"

class nsStyleSheetService final : public nsIStyleSheetService,
                                  public nsIMemoryReporter {
 public:
  typedef nsTArray<RefPtr<mozilla::StyleSheet>> SheetArray;

  nsStyleSheetService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTYLESHEETSERVICE
  NS_DECL_NSIMEMORYREPORTER

  nsresult Init();

  SheetArray* AgentStyleSheets() { return &mSheets[AGENT_SHEET]; }
  SheetArray* UserStyleSheets() { return &mSheets[USER_SHEET]; }
  SheetArray* AuthorStyleSheets() { return &mSheets[AUTHOR_SHEET]; }

  void RegisterPresShell(mozilla::PresShell* aPresShell);
  void UnregisterPresShell(mozilla::PresShell* aPresShell);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  static nsStyleSheetService* GetInstance();
  static nsStyleSheetService* gInstance;

 private:
  ~nsStyleSheetService();

  void RegisterFromEnumerator(nsICategoryManager* aManager,
                              const char* aCategory,
                              nsISimpleEnumerator* aEnumerator,
                              uint32_t aSheetType);

  int32_t FindSheetByURI(uint32_t aSheetType, nsIURI* aSheetURI);

  // Like LoadAndRegisterSheet, but doesn't notify.  If successful, the
  // new sheet will be the last sheet in mSheets[aSheetType].
  nsresult LoadAndRegisterSheetInternal(nsIURI* aSheetURI, uint32_t aSheetType);

  mozilla::Array<SheetArray, 3> mSheets;

  // Registered PresShells that will be notified when sheets are added and
  // removed from the style sheet service.
  nsTArray<RefPtr<mozilla::PresShell>> mPresShells;
};

#endif
