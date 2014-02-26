/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLayoutStylesheetCache_h__
#define nsLayoutStylesheetCache_h__

#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

class nsCSSStyleSheet;
class nsIFile;
class nsIURI;

namespace mozilla {
namespace css {
class Loader;
}
}

class nsLayoutStylesheetCache MOZ_FINAL
 : public nsIObserver
 , public nsIMemoryReporter
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER

  static nsCSSStyleSheet* ScrollbarsSheet();
  static nsCSSStyleSheet* FormsSheet();
  // This function is expected to return nullptr when the dom.forms.number
  // pref is disabled.
  static nsCSSStyleSheet* NumberControlSheet();
  static nsCSSStyleSheet* UserContentSheet();
  static nsCSSStyleSheet* UserChromeSheet();
  static nsCSSStyleSheet* UASheet();
  static nsCSSStyleSheet* QuirkSheet();
  static nsCSSStyleSheet* FullScreenOverrideSheet();

  static void Shutdown();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  nsLayoutStylesheetCache();
  ~nsLayoutStylesheetCache();

  static void EnsureGlobal();
  void InitFromProfile();
  void InitMemoryReporter();
  static void LoadSheetFile(nsIFile* aFile, nsRefPtr<nsCSSStyleSheet> &aSheet);
  static void LoadSheet(nsIURI* aURI, nsRefPtr<nsCSSStyleSheet> &aSheet,
                        bool aEnableUnsafeRules);

  static nsLayoutStylesheetCache* gStyleCache;
  static mozilla::css::Loader* gCSSLoader;
  nsRefPtr<nsCSSStyleSheet> mScrollbarsSheet;
  nsRefPtr<nsCSSStyleSheet> mFormsSheet;
  nsRefPtr<nsCSSStyleSheet> mNumberControlSheet;
  nsRefPtr<nsCSSStyleSheet> mUserContentSheet;
  nsRefPtr<nsCSSStyleSheet> mUserChromeSheet;
  nsRefPtr<nsCSSStyleSheet> mUASheet;
  nsRefPtr<nsCSSStyleSheet> mQuirkSheet;
  nsRefPtr<nsCSSStyleSheet> mFullScreenOverrideSheet;
};

#endif
