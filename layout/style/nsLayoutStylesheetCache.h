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
#include "mozilla/StyleBackendType.h"
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

  /**
   * Returns the nsLayoutStylesheetCache for the given style backend type.
   * Callers should pass in a value for aType that matches the style system
   * backend type for the style set in use.  (A process may call For
   * and obtain nsLayoutStylesheetCache objects for both backend types,
   * and a particular UA style sheet might be cached in both, one or neither
   * nsLayoutStylesheetCache.)
   */
  static nsLayoutStylesheetCache* For(mozilla::StyleBackendType aType);

  mozilla::StyleSheetHandle ScrollbarsSheet();
  mozilla::StyleSheetHandle FormsSheet();
  // This function is expected to return nullptr when the dom.forms.number
  // pref is disabled.
  mozilla::StyleSheetHandle NumberControlSheet();
  mozilla::StyleSheetHandle UserContentSheet();
  mozilla::StyleSheetHandle UserChromeSheet();
  mozilla::StyleSheetHandle UASheet();
  mozilla::StyleSheetHandle HTMLSheet();
  mozilla::StyleSheetHandle MinimalXULSheet();
  mozilla::StyleSheetHandle XULSheet();
  mozilla::StyleSheetHandle QuirkSheet();
  mozilla::StyleSheetHandle SVGSheet();
  mozilla::StyleSheetHandle MathMLSheet();
  mozilla::StyleSheetHandle CounterStylesSheet();
  mozilla::StyleSheetHandle NoScriptSheet();
  mozilla::StyleSheetHandle NoFramesSheet();
  mozilla::StyleSheetHandle ChromePreferenceSheet(nsPresContext* aPresContext);
  mozilla::StyleSheetHandle ContentPreferenceSheet(nsPresContext* aPresContext);
  mozilla::StyleSheetHandle ContentEditableSheet();
  mozilla::StyleSheetHandle DesignModeSheet();

  static void InvalidatePreferenceSheets();

  static void Shutdown();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  explicit nsLayoutStylesheetCache(mozilla::StyleBackendType aImpl);
  ~nsLayoutStylesheetCache();

  void InitFromProfile();
  void InitMemoryReporter();
  void LoadSheetURL(const char* aURL,
                    mozilla::StyleSheetHandle::RefPtr* aSheet,
                    mozilla::css::SheetParsingMode aParsingMode);
  void LoadSheetFile(nsIFile* aFile,
                     mozilla::StyleSheetHandle::RefPtr* aSheet,
                     mozilla::css::SheetParsingMode aParsingMode);
  void LoadSheet(nsIURI* aURI, mozilla::StyleSheetHandle::RefPtr* aSheet,
                 mozilla::css::SheetParsingMode aParsingMode);
  static void InvalidateSheet(mozilla::StyleSheetHandle::RefPtr* aGeckoSheet,
                              mozilla::StyleSheetHandle::RefPtr* aServoSheet);
  static void DependentPrefChanged(const char* aPref, void* aData);
  void BuildPreferenceSheet(mozilla::StyleSheetHandle::RefPtr* aSheet,
                            nsPresContext* aPresContext);

  static mozilla::StaticRefPtr<nsLayoutStylesheetCache> gStyleCache_Gecko;
  static mozilla::StaticRefPtr<nsLayoutStylesheetCache> gStyleCache_Servo;
  static mozilla::StaticRefPtr<mozilla::css::Loader> gCSSLoader_Gecko;
  static mozilla::StaticRefPtr<mozilla::css::Loader> gCSSLoader_Servo;
  mozilla::StyleBackendType mBackendType;
  mozilla::StyleSheetHandle::RefPtr mChromePreferenceSheet;
  mozilla::StyleSheetHandle::RefPtr mContentEditableSheet;
  mozilla::StyleSheetHandle::RefPtr mContentPreferenceSheet;
  mozilla::StyleSheetHandle::RefPtr mCounterStylesSheet;
  mozilla::StyleSheetHandle::RefPtr mDesignModeSheet;
  mozilla::StyleSheetHandle::RefPtr mFormsSheet;
  mozilla::StyleSheetHandle::RefPtr mHTMLSheet;
  mozilla::StyleSheetHandle::RefPtr mMathMLSheet;
  mozilla::StyleSheetHandle::RefPtr mMinimalXULSheet;
  mozilla::StyleSheetHandle::RefPtr mNoFramesSheet;
  mozilla::StyleSheetHandle::RefPtr mNoScriptSheet;
  mozilla::StyleSheetHandle::RefPtr mNumberControlSheet;
  mozilla::StyleSheetHandle::RefPtr mQuirkSheet;
  mozilla::StyleSheetHandle::RefPtr mSVGSheet;
  mozilla::StyleSheetHandle::RefPtr mScrollbarsSheet;
  mozilla::StyleSheetHandle::RefPtr mUASheet;
  mozilla::StyleSheetHandle::RefPtr mUserChromeSheet;
  mozilla::StyleSheetHandle::RefPtr mUserContentSheet;
  mozilla::StyleSheetHandle::RefPtr mXULSheet;
};

#endif
