/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutStylesheetCache.h"

#include "nsAppDirectoryServiceDefs.h"
#include "mozilla/css/Loader.h"
#include "nsIFile.h"
#include "nsIMemoryReporter.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsIXULRuntime.h"
#include "nsCSSStyleSheet.h"

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(LayoutStyleSheetCacheMallocSizeOf)

static int64_t
GetStylesheetCacheSize()
{
  return nsLayoutStylesheetCache::SizeOfIncludingThis(
           LayoutStyleSheetCacheMallocSizeOf);
}

NS_MEMORY_REPORTER_IMPLEMENT(StyleSheetCache,
  "explicit/layout/style-sheet-cache",
  KIND_HEAP,
  nsIMemoryReporter::UNITS_BYTES,
  GetStylesheetCacheSize,
  "Memory used for some built-in style sheets.")

NS_IMPL_ISUPPORTS1(nsLayoutStylesheetCache, nsIObserver)

nsresult
nsLayoutStylesheetCache::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const PRUnichar* aData)
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
  }
  else {
    NS_NOTREACHED("Unexpected observer topic.");
  }
  return NS_OK;
}

nsCSSStyleSheet*
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

nsCSSStyleSheet*
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

nsCSSStyleSheet*
nsLayoutStylesheetCache::UserContentSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mUserContentSheet;
}

nsCSSStyleSheet*
nsLayoutStylesheetCache::UserChromeSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mUserChromeSheet;
}

nsCSSStyleSheet*
nsLayoutStylesheetCache::UASheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mUASheet;
}

nsCSSStyleSheet*
nsLayoutStylesheetCache::QuirkSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mQuirkSheet;
}

nsCSSStyleSheet*
nsLayoutStylesheetCache::FullScreenOverrideSheet()
{
  EnsureGlobal();
  if (!gStyleCache)
    return nullptr;

  return gStyleCache->mFullScreenOverrideSheet;
}

void
nsLayoutStylesheetCache::Shutdown()
{
  NS_IF_RELEASE(gCSSLoader);
  NS_IF_RELEASE(gStyleCache);
}

size_t
nsLayoutStylesheetCache::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf)
{
  if (!nsLayoutStylesheetCache::gStyleCache) {
    return 0;
  }

  return nsLayoutStylesheetCache::gStyleCache->
      SizeOfIncludingThisHelper(aMallocSizeOf);
}

size_t
nsLayoutStylesheetCache::SizeOfIncludingThisHelper(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  #define MEASURE(s) n += s ? s->SizeOfIncludingThis(aMallocSizeOf) : 0;

  MEASURE(mScrollbarsSheet);
  MEASURE(mFormsSheet);
  MEASURE(mUserContentSheet);
  MEASURE(mUserChromeSheet);
  MEASURE(mUASheet);
  MEASURE(mQuirkSheet);
  MEASURE(mFullScreenOverrideSheet);

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

  mReporter = new NS_MEMORY_REPORTER_NAME(StyleSheetCache);
  (void)::NS_RegisterMemoryReporter(mReporter);
}

nsLayoutStylesheetCache::~nsLayoutStylesheetCache()
{
  (void)::NS_UnregisterMemoryReporter(mReporter);
  mReporter = nullptr;
}

void
nsLayoutStylesheetCache::EnsureGlobal()
{
  if (gStyleCache) return;

  gStyleCache = new nsLayoutStylesheetCache();
  if (!gStyleCache) return;

  NS_ADDREF(gStyleCache);
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
nsLayoutStylesheetCache::LoadSheetFile(nsIFile* aFile, nsRefPtr<nsCSSStyleSheet> &aSheet)
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
                                   nsRefPtr<nsCSSStyleSheet> &aSheet,
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
