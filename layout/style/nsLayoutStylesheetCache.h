/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLayoutStylesheetCache_h__
#define nsLayoutStylesheetCache_h__

#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/css/Loader.h"

class nsIFile;
class nsIURI;

namespace mozilla {
class CSSStyleSheet;
}  // namespace mozilla

namespace mozilla {
namespace css {

// Enum defining how error should be handled.
enum FailureAction { eCrash = 0, eLogToConsole };

}  // namespace css
}  // namespace mozilla

class nsLayoutStylesheetCache final : public nsIObserver,
                                      public nsIMemoryReporter {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER

  static nsLayoutStylesheetCache* Singleton();

#define STYLE_SHEET(identifier_, url_, lazy_) \
  mozilla::StyleSheet* identifier_##Sheet();
#include "mozilla/UserAgentStyleSheetList.h"
#undef STYLE_SHEET

  mozilla::StyleSheet* UserContentSheet();
  mozilla::StyleSheet* UserChromeSheet();
  mozilla::StyleSheet* ChromePreferenceSheet(nsPresContext* aPresContext);
  mozilla::StyleSheet* ContentPreferenceSheet(nsPresContext* aPresContext);

  static void InvalidatePreferenceSheets();

  static void Shutdown();

  static void SetUserContentCSSURL(nsIURI* aURI);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  nsLayoutStylesheetCache();
  ~nsLayoutStylesheetCache();

  void InitFromProfile();
  void InitMemoryReporter();
  void LoadSheetURL(const char* aURL, RefPtr<mozilla::StyleSheet>* aSheet,
                    mozilla::css::SheetParsingMode aParsingMode,
                    mozilla::css::FailureAction aFailureAction);
  void LoadSheetFile(nsIFile* aFile, RefPtr<mozilla::StyleSheet>* aSheet,
                     mozilla::css::SheetParsingMode aParsingMode,
                     mozilla::css::FailureAction aFailureAction);
  void LoadSheet(nsIURI* aURI, RefPtr<mozilla::StyleSheet>* aSheet,
                 mozilla::css::SheetParsingMode aParsingMode,
                 mozilla::css::FailureAction aFailureAction);
  void BuildPreferenceSheet(RefPtr<mozilla::StyleSheet>* aSheet,
                            nsPresContext* aPresContext);

  static mozilla::StaticRefPtr<nsLayoutStylesheetCache> gStyleCache;
  static mozilla::StaticRefPtr<mozilla::css::Loader> gCSSLoader;
  static mozilla::StaticRefPtr<nsIURI> gUserContentSheetURL;
  RefPtr<mozilla::StyleSheet> mChromePreferenceSheet;
  RefPtr<mozilla::StyleSheet> mContentEditableSheet;
  RefPtr<mozilla::StyleSheet> mContentPreferenceSheet;
  RefPtr<mozilla::StyleSheet> mCounterStylesSheet;
  RefPtr<mozilla::StyleSheet> mDesignModeSheet;
  RefPtr<mozilla::StyleSheet> mFormsSheet;
  RefPtr<mozilla::StyleSheet> mHTMLSheet;
  RefPtr<mozilla::StyleSheet> mMathMLSheet;
  RefPtr<mozilla::StyleSheet> mMinimalXULSheet;
  RefPtr<mozilla::StyleSheet> mNoFramesSheet;
  RefPtr<mozilla::StyleSheet> mNoScriptSheet;
  RefPtr<mozilla::StyleSheet> mQuirkSheet;
  RefPtr<mozilla::StyleSheet> mSVGSheet;
  RefPtr<mozilla::StyleSheet> mScrollbarsSheet;
  RefPtr<mozilla::StyleSheet> mUASheet;
  RefPtr<mozilla::StyleSheet> mUserChromeSheet;
  RefPtr<mozilla::StyleSheet> mUserContentSheet;
  RefPtr<mozilla::StyleSheet> mXULSheet;
};

#endif
