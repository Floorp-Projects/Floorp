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
#include "mozilla/StaticPtr.h"
#include "mozilla/css/Loader.h"

class nsIFile;
class nsIURI;

namespace mozilla {
class CSSStyleSheet;
} // namespace mozilla

class nsLayoutStylesheetCache final
 : public nsIObserver
 , public nsIMemoryReporter
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER

  static mozilla::CSSStyleSheet* ScrollbarsSheet();
  static mozilla::CSSStyleSheet* FormsSheet();
  // This function is expected to return nullptr when the dom.forms.number
  // pref is disabled.
  static mozilla::CSSStyleSheet* NumberControlSheet();
  static mozilla::CSSStyleSheet* UserContentSheet();
  static mozilla::CSSStyleSheet* UserChromeSheet();
  static mozilla::CSSStyleSheet* UASheet();
  static mozilla::CSSStyleSheet* HTMLSheet();
  static mozilla::CSSStyleSheet* MinimalXULSheet();
  static mozilla::CSSStyleSheet* XULSheet();
  static mozilla::CSSStyleSheet* QuirkSheet();
  static mozilla::CSSStyleSheet* SVGSheet();
  static mozilla::CSSStyleSheet* MathMLSheet();
  static mozilla::CSSStyleSheet* CounterStylesSheet();
  static mozilla::CSSStyleSheet* NoScriptSheet();
  static mozilla::CSSStyleSheet* NoFramesSheet();
  static mozilla::CSSStyleSheet* ChromePreferenceSheet(nsPresContext* aPresContext);
  static mozilla::CSSStyleSheet* ContentPreferenceSheet(nsPresContext* aPresContext);
  static mozilla::CSSStyleSheet* ContentEditableSheet();
  static mozilla::CSSStyleSheet* DesignModeSheet();

  static void InvalidatePreferenceSheets();

  static void Shutdown();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  nsLayoutStylesheetCache();
  ~nsLayoutStylesheetCache();

  static void EnsureGlobal();
  void InitFromProfile();
  void InitMemoryReporter();
  static void LoadSheetURL(const char* aURL,
                           RefPtr<mozilla::CSSStyleSheet>& aSheet,
                           mozilla::css::SheetParsingMode aParsingMode);
  static void LoadSheetFile(nsIFile* aFile,
                            RefPtr<mozilla::CSSStyleSheet>& aSheet,
                            mozilla::css::SheetParsingMode aParsingMode);
  static void LoadSheet(nsIURI* aURI, RefPtr<mozilla::CSSStyleSheet>& aSheet,
                        mozilla::css::SheetParsingMode aParsingMode);
  static void InvalidateSheet(RefPtr<mozilla::CSSStyleSheet>& aSheet);
  static void DependentPrefChanged(const char* aPref, void* aData);
  void BuildPreferenceSheet(RefPtr<mozilla::CSSStyleSheet>& aSheet,
                            nsPresContext* aPresContext);

  static mozilla::StaticRefPtr<nsLayoutStylesheetCache> gStyleCache;
  static mozilla::css::Loader* gCSSLoader;
  RefPtr<mozilla::CSSStyleSheet> mChromePreferenceSheet;
  RefPtr<mozilla::CSSStyleSheet> mContentEditableSheet;
  RefPtr<mozilla::CSSStyleSheet> mContentPreferenceSheet;
  RefPtr<mozilla::CSSStyleSheet> mCounterStylesSheet;
  RefPtr<mozilla::CSSStyleSheet> mDesignModeSheet;
  RefPtr<mozilla::CSSStyleSheet> mFormsSheet;
  RefPtr<mozilla::CSSStyleSheet> mHTMLSheet;
  RefPtr<mozilla::CSSStyleSheet> mMathMLSheet;
  RefPtr<mozilla::CSSStyleSheet> mMinimalXULSheet;
  RefPtr<mozilla::CSSStyleSheet> mNoFramesSheet;
  RefPtr<mozilla::CSSStyleSheet> mNoScriptSheet;
  RefPtr<mozilla::CSSStyleSheet> mNumberControlSheet;
  RefPtr<mozilla::CSSStyleSheet> mQuirkSheet;
  RefPtr<mozilla::CSSStyleSheet> mSVGSheet;
  RefPtr<mozilla::CSSStyleSheet> mScrollbarsSheet;
  RefPtr<mozilla::CSSStyleSheet> mUASheet;
  RefPtr<mozilla::CSSStyleSheet> mUserChromeSheet;
  RefPtr<mozilla::CSSStyleSheet> mUserContentSheet;
  RefPtr<mozilla::CSSStyleSheet> mXULSheet;
};

#endif
