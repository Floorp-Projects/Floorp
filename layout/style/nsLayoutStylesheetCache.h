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
#include "mozilla/StyleBackendType.h"
#include "mozilla/css/Loader.h"

class nsIFile;
class nsIURI;

namespace mozilla {
class CSSStyleSheet;
} // namespace mozilla

namespace mozilla {
namespace css {

// Enum defining how error should be handled.
enum FailureAction {
  eCrash = 0,
  eLogToConsole
};

}
}

class nsLayoutStylesheetCache final
 : public nsIObserver
 , public nsIMemoryReporter
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER

  /**
   * Returns the nsLayoutStylesheetCache for the given style backend type.
   * Callers should pass in a value for aType that matches the style system
   * backend type for the style set in use.  (A process may call For
   * and obtain nsLayoutStylesheetCache objects for both backend types,
   * and a particular UA style sheet might be cached in both, one or neither
   * nsLayoutStylesheetCache.)
   */
  static nsLayoutStylesheetCache* For(mozilla::StyleBackendType aType);

  mozilla::StyleSheet* ScrollbarsSheet();
  mozilla::StyleSheet* FormsSheet();
  // This function is expected to return nullptr when the dom.forms.number
  // pref is disabled.
  mozilla::StyleSheet* NumberControlSheet();
  mozilla::StyleSheet* UserContentSheet();
  mozilla::StyleSheet* UserChromeSheet();
  mozilla::StyleSheet* UASheet();
  mozilla::StyleSheet* HTMLSheet();
  mozilla::StyleSheet* MinimalXULSheet();
  mozilla::StyleSheet* XULSheet();
  mozilla::StyleSheet* QuirkSheet();
  mozilla::StyleSheet* SVGSheet();
  mozilla::StyleSheet* MathMLSheet();
  mozilla::StyleSheet* CounterStylesSheet();
  mozilla::StyleSheet* NoScriptSheet();
  mozilla::StyleSheet* NoFramesSheet();
  mozilla::StyleSheet* ChromePreferenceSheet(nsPresContext* aPresContext);
  mozilla::StyleSheet* ContentPreferenceSheet(nsPresContext* aPresContext);
  mozilla::StyleSheet* ContentEditableSheet();
  mozilla::StyleSheet* DesignModeSheet();

  static void InvalidatePreferenceSheets();

  static void Shutdown();

  static void SetUserContentCSSURL(nsIURI* aURI);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  explicit nsLayoutStylesheetCache(mozilla::StyleBackendType aImpl);
  ~nsLayoutStylesheetCache();

  void InitFromProfile();
  void InitMemoryReporter();
  void LoadSheetURL(const char* aURL,
                    RefPtr<mozilla::StyleSheet>* aSheet,
                    mozilla::css::SheetParsingMode aParsingMode,
                    mozilla::css::FailureAction aFailureAction);
  void LoadSheetFile(nsIFile* aFile,
                     RefPtr<mozilla::StyleSheet>* aSheet,
                     mozilla::css::SheetParsingMode aParsingMode,
                     mozilla::css::FailureAction aFailureAction);
  void LoadSheet(nsIURI* aURI, RefPtr<mozilla::StyleSheet>* aSheet,
                 mozilla::css::SheetParsingMode aParsingMode,
                 mozilla::css::FailureAction aFailureAction);
  static void InvalidateSheet(RefPtr<mozilla::StyleSheet>* aGeckoSheet,
                              RefPtr<mozilla::StyleSheet>* aServoSheet);
  static void DependentPrefChanged(const char* aPref, void* aData);
  void BuildPreferenceSheet(RefPtr<mozilla::StyleSheet>* aSheet,
                            nsPresContext* aPresContext);

  static mozilla::StaticRefPtr<nsLayoutStylesheetCache> gStyleCache_Gecko;
  static mozilla::StaticRefPtr<nsLayoutStylesheetCache> gStyleCache_Servo;
  static mozilla::StaticRefPtr<mozilla::css::Loader> gCSSLoader_Gecko;
  static mozilla::StaticRefPtr<mozilla::css::Loader> gCSSLoader_Servo;
  static mozilla::StaticRefPtr<nsIURI> gUserContentSheetURL;
  mozilla::StyleBackendType mBackendType;
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
  RefPtr<mozilla::StyleSheet> mNumberControlSheet;
  RefPtr<mozilla::StyleSheet> mQuirkSheet;
  RefPtr<mozilla::StyleSheet> mSVGSheet;
  RefPtr<mozilla::StyleSheet> mScrollbarsSheet;
  RefPtr<mozilla::StyleSheet> mUASheet;
  RefPtr<mozilla::StyleSheet> mUserChromeSheet;
  RefPtr<mozilla::StyleSheet> mUserContentSheet;
  RefPtr<mozilla::StyleSheet> mXULSheet;
};

#endif
