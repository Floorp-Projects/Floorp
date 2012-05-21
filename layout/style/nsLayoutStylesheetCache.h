/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLayoutStylesheetCache_h__
#define nsLayoutStylesheetCache_h__

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsAutoPtr.h"

class nsIFile;
class nsCSSStyleSheet;
class nsIURI;

namespace mozilla {
namespace css {
class Loader;
}
}

class nsIMemoryReporter;

class nsLayoutStylesheetCache
 : public nsIObserver
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static nsCSSStyleSheet* ScrollbarsSheet();
  static nsCSSStyleSheet* FormsSheet();
  static nsCSSStyleSheet* UserContentSheet();
  static nsCSSStyleSheet* UserChromeSheet();
  static nsCSSStyleSheet* UASheet();
  static nsCSSStyleSheet* QuirkSheet();
  static nsCSSStyleSheet* FullScreenOverrideSheet();

  static void Shutdown();

  static size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf);

private:
  nsLayoutStylesheetCache();
  ~nsLayoutStylesheetCache();

  static void EnsureGlobal();
  void InitFromProfile();
  static void LoadSheetFile(nsIFile* aFile, nsRefPtr<nsCSSStyleSheet> &aSheet);
  static void LoadSheet(nsIURI* aURI, nsRefPtr<nsCSSStyleSheet> &aSheet,
                        bool aEnableUnsafeRules);

  size_t SizeOfIncludingThisHelper(nsMallocSizeOfFun aMallocSizeOf) const;

  static nsLayoutStylesheetCache* gStyleCache;
  static mozilla::css::Loader* gCSSLoader;
  nsRefPtr<nsCSSStyleSheet> mScrollbarsSheet;
  nsRefPtr<nsCSSStyleSheet> mFormsSheet;
  nsRefPtr<nsCSSStyleSheet> mUserContentSheet;
  nsRefPtr<nsCSSStyleSheet> mUserChromeSheet;
  nsRefPtr<nsCSSStyleSheet> mUASheet;
  nsRefPtr<nsCSSStyleSheet> mQuirkSheet;
  nsRefPtr<nsCSSStyleSheet> mFullScreenOverrideSheet;

  nsIMemoryReporter* mSheetsReporter;
};

#endif
