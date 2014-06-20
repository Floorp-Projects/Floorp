/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutStylesheetCache.h"

#include "nsAppDirectoryServiceDefs.h"
#include "mozilla/CSSStyleSheet.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/css/Loader.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsIXULRuntime.h"

using namespace mozilla;

static bool sNumberControlEnabled;

#define NUMBER_CONTROL_PREF "dom.forms.number"

NS_IMPL_ISUPPORTS(
  nsLayoutStylesheetCache, nsIObserver, nsIMemoryReporter)

nsresult
nsLayoutStylesheetCache::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const char16_t* aData)
{
  if (!strcmp(aTopic, "profile-before-change")) {
    mUserContentSheet = nullptr;
    mUserChromeSheet  = nullptr;
  }
  else if (!strcmp(aTopic, "profile-do-change")) {
    InitFromProfile();
  }
  else if (strcmp(aTopic, "chrome-flush-skin-caches") == 0 ||
           strcmp(aTopic, "chrome-flush-caches") == 0) {
    mScrollbarsSheet = nullptr;
    mFormsSheet = nullptr;
    mNumberControlSheet = nullptr;
  }
  else {
    NS_NOTREACHED("Unexpected observer topic.");
  }
  return NS_OK;
}

CSSStyleSheet*
nsLayoutStylesheetCache::ScrollbarsSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  if (!gStyleCache->mScrollbarsSheet) {
    nsCOMPtr<nsIURI> sheetURI;
    NS_NewURI(getter_AddRefs(sheetURI),
              NS_LITERAL_CSTRING("chrome://global/skin/scrollbars.css"));

    // Scrollbars don't need access to unsafe rules
    if (sheetURI)
      LoadSheet(sheetURI, gStyleCache->mScrollbarsSheet, false);
    NS_ASSERTION(gStyleCache->mScrollbarsSheet, "Could not load scrollbars.css.");
  }

  return gStyleCache->mScrollbarsSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::FormsSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  if (!gStyleCache->mFormsSheet) {
    nsCOMPtr<nsIURI> sheetURI;
      NS_NewURI(getter_AddRefs(sheetURI),
                NS_LITERAL_CSTRING("resource://gre-resources/forms.css"));

    // forms.css needs access to unsafe rules
    if (sheetURI)
      LoadSheet(sheetURI, gStyleCache->mFormsSheet, true);

    NS_ASSERTION(gStyleCache->mFormsSheet, "Could not load forms.css.");
  }

  return gStyleCache->mFormsSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::NumberControlSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  if (!sNumberControlEnabled) {
    return nullptr;
  }

  if (!gStyleCache->mNumberControlSheet) {
    nsCOMPtr<nsIURI> sheetURI;
    NS_NewURI(getter_AddRefs(sheetURI),
              NS_LITERAL_CSTRING("resource://gre-resources/number-control.css"));

    if (sheetURI)
      LoadSheet(sheetURI, gStyleCache->mNumberControlSheet, false);

    NS_ASSERTION(gStyleCache->mNumberControlSheet, "Could not load number-control.css");
  }

  return gStyleCache->mNumberControlSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::UserContentSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mUserContentSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::UserChromeSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mUserChromeSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::UASheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mUASheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::HTMLSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mHTMLSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::MinimalXULSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mMinimalXULSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::XULSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mXULSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::QuirkSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mQuirkSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::FullScreenOverrideSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mFullScreenOverrideSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::SVGSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mSVGSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::MathMLSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  if (!gStyleCache->mMathMLSheet) {
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), "resource://gre-resources/mathml.css");
    if (uri) {
      LoadSheet(uri, gStyleCache->mMathMLSheet, true);
    }
    NS_ASSERTION(gStyleCache->mMathMLSheet, "Could not load mathml.css");
  }

  return gStyleCache->mMathMLSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::CounterStylesSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mCounterStylesSheet;
}

void
nsLayoutStylesheetCache::Shutdown()
{
  NS_IF_RELEASE(gCSSLoader);
  NS_IF_RELEASE(gStyleCache);
}

MOZ_DEFINE_MALLOC_SIZE_OF(LayoutStylesheetCacheMallocSizeOf)

NS_IMETHODIMP
nsLayoutStylesheetCache::CollectReports(nsIHandleReportCallback* aHandleReport,
                                        nsISupports* aData, bool aAnonymize)
{
  return MOZ_COLLECT_REPORT(
    "explicit/layout/style-sheet-cache", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(LayoutStylesheetCacheMallocSizeOf),
    "Memory used for some built-in style sheets.");
}


size_t
nsLayoutStylesheetCache::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  #define MEASURE(s) n += s ? s->SizeOfIncludingThis(aMallocSizeOf) : 0;

  MEASURE(mScrollbarsSheet);
  MEASURE(mFormsSheet);
  MEASURE(mNumberControlSheet);
  MEASURE(mUserContentSheet);
  MEASURE(mUserChromeSheet);
  MEASURE(mUASheet);
  MEASURE(mHTMLSheet);
  MEASURE(mMinimalXULSheet);
  MEASURE(mXULSheet);
  MEASURE(mQuirkSheet);
  MEASURE(mFullScreenOverrideSheet);
  MEASURE(mSVGSheet);
  MEASURE(mCounterStylesSheet);
  if (mMathMLSheet) {
    MEASURE(mMathMLSheet);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - gCSSLoader

  return n;
}

nsLayoutStylesheetCache::nsLayoutStylesheetCache()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  NS_ASSERTION(obsSvc, "No global observer service?");

  if (obsSvc) {
    obsSvc->AddObserver(this, "profile-before-change", false);
    obsSvc->AddObserver(this, "profile-do-change", false);
    obsSvc->AddObserver(this, "chrome-flush-skin-caches", false);
    obsSvc->AddObserver(this, "chrome-flush-caches", false);
  }

  InitFromProfile();

  // And make sure that we load our UA sheets.  No need to do this
  // per-profile, since they're profile-invariant.
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "resource://gre-resources/ua.css");
  if (uri) {
    LoadSheet(uri, mUASheet, true);
  }
  NS_ASSERTION(mUASheet, "Could not load ua.css");

  NS_NewURI(getter_AddRefs(uri), "resource://gre-resources/html.css");
  if (uri) {
    LoadSheet(uri, mHTMLSheet, true);
  }
  NS_ASSERTION(mHTMLSheet, "Could not load xul.css");

  NS_NewURI(getter_AddRefs(uri), "chrome://global/content/minimal-xul.css");
  if (uri) {
    LoadSheet(uri, mMinimalXULSheet, true);
  }
  NS_ASSERTION(mMinimalXULSheet, "Could not load minimal-xul.css");

  NS_NewURI(getter_AddRefs(uri), "chrome://global/content/xul.css");
  if (uri) {
    LoadSheet(uri, mXULSheet, true);
  }
  NS_ASSERTION(mXULSheet, "Could not load xul.css");

  NS_NewURI(getter_AddRefs(uri), "resource://gre-resources/quirk.css");
  if (uri) {
    LoadSheet(uri, mQuirkSheet, true);
  }
  NS_ASSERTION(mQuirkSheet, "Could not load quirk.css");

  NS_NewURI(getter_AddRefs(uri), "resource://gre-resources/full-screen-override.css");
  if (uri) {
    LoadSheet(uri, mFullScreenOverrideSheet, true);
  }
  NS_ASSERTION(mFullScreenOverrideSheet, "Could not load full-screen-override.css");

  NS_NewURI(getter_AddRefs(uri), "resource://gre/res/svg.css");
  if (uri) {
    LoadSheet(uri, mSVGSheet, true);
  }
  NS_ASSERTION(mSVGSheet, "Could not load svg.css");

  NS_NewURI(getter_AddRefs(uri), "resource://gre-resources/counterstyles.css");
  if (uri) {
    LoadSheet(uri, mCounterStylesSheet, true);
  }
  NS_ASSERTION(mCounterStylesSheet, "Could not load counterstyles.css");

  // mMathMLSheet is created on-demand since its use is rare. This helps save
  // memory for Firefox OS apps.
}

nsLayoutStylesheetCache::~nsLayoutStylesheetCache()
{
  mozilla::UnregisterWeakMemoryReporter(this);
  gStyleCache = nullptr;
}

void
nsLayoutStylesheetCache::InitMemoryReporter()
{
  mozilla::RegisterWeakMemoryReporter(this);
}

void
nsLayoutStylesheetCache::EnsureGlobal()
{
  if (gStyleCache) return;

  gStyleCache = new nsLayoutStylesheetCache();
  if (!gStyleCache) return;

  NS_ADDREF(gStyleCache);

  gStyleCache->InitMemoryReporter();

  Preferences::AddBoolVarCache(&sNumberControlEnabled, NUMBER_CONTROL_PREF,
                               true);
}

void
nsLayoutStylesheetCache::InitFromProfile()
{
  nsCOMPtr<nsIXULRuntime> appInfo = do_GetService("@mozilla.org/xre/app-info;1");
  if (appInfo) {
    bool inSafeMode = false;
    appInfo->GetInSafeMode(&inSafeMode);
    if (inSafeMode)
      return;
  }
  nsCOMPtr<nsIFile> contentFile;
  nsCOMPtr<nsIFile> chromeFile;

  NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR,
                         getter_AddRefs(contentFile));
  if (!contentFile) {
    // if we don't have a profile yet, that's OK!
    return;
  }

  contentFile->Clone(getter_AddRefs(chromeFile));
  if (!chromeFile) return;

  contentFile->Append(NS_LITERAL_STRING("userContent.css"));
  chromeFile->Append(NS_LITERAL_STRING("userChrome.css"));

  LoadSheetFile(contentFile, mUserContentSheet);
  LoadSheetFile(chromeFile, mUserChromeSheet);
}

void
nsLayoutStylesheetCache::LoadSheetFile(nsIFile* aFile, nsRefPtr<CSSStyleSheet>& aSheet)
{
  bool exists = false;
  aFile->Exists(&exists);

  if (!exists) return;

  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), aFile);

  LoadSheet(uri, aSheet, false);
}

void
nsLayoutStylesheetCache::LoadSheet(nsIURI* aURI,
                                   nsRefPtr<CSSStyleSheet>& aSheet,
                                   bool aEnableUnsafeRules)
{
  if (!aURI) {
    NS_ERROR("Null URI. Out of memory?");
    return;
  }

  if (!gCSSLoader) {
    gCSSLoader = new mozilla::css::Loader();
    NS_IF_ADDREF(gCSSLoader);
  }

  if (gCSSLoader) {
    gCSSLoader->LoadSheetSync(aURI, aEnableUnsafeRules, true,
                              getter_AddRefs(aSheet));
  }
}

nsLayoutStylesheetCache*
nsLayoutStylesheetCache::gStyleCache = nullptr;

mozilla::css::Loader*
nsLayoutStylesheetCache::gCSSLoader = nullptr;
