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
#include "nsPrintfCString.h"

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

  if (!gStyleCache->mScrollbarsSheet) {
    // Scrollbars don't need access to unsafe rules
    LoadSheetURL("chrome://global/skin/scrollbars.css",
                 gStyleCache->mScrollbarsSheet, false);
  }

  return gStyleCache->mScrollbarsSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::FormsSheet()
{
  EnsureGlobal();

  if (!gStyleCache->mFormsSheet) {
    // forms.css needs access to unsafe rules
    LoadSheetURL("resource://gre-resources/forms.css",
                 gStyleCache->mFormsSheet, true);
  }

  return gStyleCache->mFormsSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::NumberControlSheet()
{
  EnsureGlobal();

  if (!sNumberControlEnabled) {
    return nullptr;
  }

  if (!gStyleCache->mNumberControlSheet) {
    LoadSheetURL("resource://gre-resources/number-control.css",
                 gStyleCache->mNumberControlSheet, false);
  }

  return gStyleCache->mNumberControlSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::UserContentSheet()
{
  EnsureGlobal();
  return gStyleCache->mUserContentSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::UserChromeSheet()
{
  EnsureGlobal();
  return gStyleCache->mUserChromeSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::UASheet()
{
  EnsureGlobal();
  return gStyleCache->mUASheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::HTMLSheet()
{
  EnsureGlobal();
  return gStyleCache->mHTMLSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::MinimalXULSheet()
{
  EnsureGlobal();
  return gStyleCache->mMinimalXULSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::XULSheet()
{
  EnsureGlobal();
  return gStyleCache->mXULSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::QuirkSheet()
{
  EnsureGlobal();
  return gStyleCache->mQuirkSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::FullScreenOverrideSheet()
{
  EnsureGlobal();
  return gStyleCache->mFullScreenOverrideSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::SVGSheet()
{
  EnsureGlobal();
  return gStyleCache->mSVGSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::MathMLSheet()
{
  EnsureGlobal();

  if (!gStyleCache->mMathMLSheet) {
    LoadSheetURL("resource://gre-resources/mathml.css",
                 gStyleCache->mMathMLSheet, true);
  }

  return gStyleCache->mMathMLSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::CounterStylesSheet()
{
  EnsureGlobal();

  return gStyleCache->mCounterStylesSheet;
}

void
nsLayoutStylesheetCache::Shutdown()
{
  NS_IF_RELEASE(gCSSLoader);
  gStyleCache = nullptr;
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

  MEASURE(mCounterStylesSheet);
  MEASURE(mFormsSheet);
  MEASURE(mFullScreenOverrideSheet);
  MEASURE(mHTMLSheet);
  MEASURE(mMathMLSheet);
  MEASURE(mMinimalXULSheet);
  MEASURE(mNumberControlSheet);
  MEASURE(mQuirkSheet);
  MEASURE(mSVGSheet);
  MEASURE(mScrollbarsSheet);
  MEASURE(mUASheet);
  MEASURE(mUserChromeSheet);
  MEASURE(mUserContentSheet);
  MEASURE(mXULSheet);

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
  LoadSheetURL("resource://gre-resources/counterstyles.css",
               mCounterStylesSheet, true);
  LoadSheetURL("resource://gre-resources/full-screen-override.css",
               mFullScreenOverrideSheet, true);
  LoadSheetURL("resource://gre-resources/html.css",
               mHTMLSheet, true);
  LoadSheetURL("chrome://global/content/minimal-xul.css",
               mMinimalXULSheet, true);
  LoadSheetURL("resource://gre-resources/quirk.css",
               mQuirkSheet, true);
  LoadSheetURL("resource://gre/res/svg.css",
               mSVGSheet, true);
  LoadSheetURL("resource://gre-resources/ua.css",
               mUASheet, true);
  LoadSheetURL("chrome://global/content/xul.css",
               mXULSheet, true);

  // The remaining sheets are created on-demand since their use is rarer. This
  // helps save memory for Firefox OS apps.
}

nsLayoutStylesheetCache::~nsLayoutStylesheetCache()
{
  mozilla::UnregisterWeakMemoryReporter(this);
  MOZ_ASSERT(!gStyleCache);
}

void
nsLayoutStylesheetCache::InitMemoryReporter()
{
  mozilla::RegisterWeakMemoryReporter(this);
}

void
nsLayoutStylesheetCache::EnsureGlobal()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gStyleCache) return;

  gStyleCache = new nsLayoutStylesheetCache();

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

/* static */ void
nsLayoutStylesheetCache::LoadSheetURL(const char* aURL,
                                      nsRefPtr<CSSStyleSheet>& aSheet,
                                      bool aEnableUnsafeRules)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), aURL);
  LoadSheet(uri, aSheet, aEnableUnsafeRules);
  if (!aSheet) {
    NS_ERROR(nsPrintfCString("Could not load %s", aURL).get());
  }
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

static void
ErrorLoadingBuiltinSheet(nsIURI* aURI, const char* aMsg)
{
  nsAutoCString spec;
  if (aURI) {
    aURI->GetSpec(spec);
  }
  NS_RUNTIMEABORT(nsPrintfCString("%s loading built-in stylesheet '%s'",
                                  aMsg, spec.get()).get());
}

void
nsLayoutStylesheetCache::LoadSheet(nsIURI* aURI,
                                   nsRefPtr<CSSStyleSheet>& aSheet,
                                   bool aEnableUnsafeRules)
{
  if (!aURI) {
    ErrorLoadingBuiltinSheet(aURI, "null URI");
    return;
  }

  if (!gCSSLoader) {
    gCSSLoader = new mozilla::css::Loader();
    NS_IF_ADDREF(gCSSLoader);
    if (!gCSSLoader) {
      ErrorLoadingBuiltinSheet(aURI, "no Loader");
      return;
    }
  }


  nsresult rv = gCSSLoader->LoadSheetSync(aURI, aEnableUnsafeRules, true,
                                          getter_AddRefs(aSheet));
  if (NS_FAILED(rv)) {
    ErrorLoadingBuiltinSheet(aURI,
      nsPrintfCString("LoadSheetSync failed with error %x", rv).get());
  }
}

mozilla::StaticRefPtr<nsLayoutStylesheetCache>
nsLayoutStylesheetCache::gStyleCache;

mozilla::css::Loader*
nsLayoutStylesheetCache::gCSSLoader = nullptr;
