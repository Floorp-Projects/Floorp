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
                 gStyleCache->mNumberControlSheet, true);
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

  if (!gStyleCache->mUASheet) {
    LoadSheetURL("resource://gre-resources/ua.css",
                 gStyleCache->mUASheet, true);
  }

  return gStyleCache->mUASheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::HTMLSheet()
{
  EnsureGlobal();

  if (!gStyleCache->mHTMLSheet) {
    LoadSheetURL("resource://gre-resources/html.css",
                 gStyleCache->mHTMLSheet, true);
  }

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

CSSStyleSheet*
nsLayoutStylesheetCache::NoScriptSheet()
{
  EnsureGlobal();

  if (!gStyleCache->mNoScriptSheet) {
    LoadSheetURL("resource://gre-resources/noscript.css",
                 gStyleCache->mNoScriptSheet, true);
  }

  return gStyleCache->mNoScriptSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::NoFramesSheet()
{
  EnsureGlobal();

  if (!gStyleCache->mNoFramesSheet) {
    LoadSheetURL("resource://gre-resources/noframes.css",
                 gStyleCache->mNoFramesSheet, true);
  }

  return gStyleCache->mNoFramesSheet;
}

/* static */ CSSStyleSheet*
nsLayoutStylesheetCache::ChromePreferenceSheet(nsPresContext* aPresContext)
{
  EnsureGlobal();

  if (!gStyleCache->mChromePreferenceSheet) {
    gStyleCache->BuildPreferenceSheet(gStyleCache->mChromePreferenceSheet,
                                      aPresContext);
  }

  return gStyleCache->mChromePreferenceSheet;
}

/* static */ CSSStyleSheet*
nsLayoutStylesheetCache::ContentPreferenceSheet(nsPresContext* aPresContext)
{
  EnsureGlobal();

  if (!gStyleCache->mContentPreferenceSheet) {
    gStyleCache->BuildPreferenceSheet(gStyleCache->mContentPreferenceSheet,
                                      aPresContext);
  }

  return gStyleCache->mContentPreferenceSheet;
}

/* static */ CSSStyleSheet*
nsLayoutStylesheetCache::ContentEditableSheet()
{
  EnsureGlobal();

  if (!gStyleCache->mContentEditableSheet) {
    LoadSheetURL("resource://gre/res/contenteditable.css",
                 gStyleCache->mContentEditableSheet, true);
  }

  return gStyleCache->mContentEditableSheet;
}

/* static */ CSSStyleSheet*
nsLayoutStylesheetCache::DesignModeSheet()
{
  EnsureGlobal();

  if (!gStyleCache->mDesignModeSheet) {
    LoadSheetURL("resource://gre/res/designmode.css",
                 gStyleCache->mDesignModeSheet, true);
  }

  return gStyleCache->mDesignModeSheet;
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

  MEASURE(mChromePreferenceSheet);
  MEASURE(mContentEditableSheet);
  MEASURE(mContentPreferenceSheet);
  MEASURE(mCounterStylesSheet);
  MEASURE(mDesignModeSheet);
  MEASURE(mFormsSheet);
  MEASURE(mFullScreenOverrideSheet);
  MEASURE(mHTMLSheet);
  MEASURE(mMathMLSheet);
  MEASURE(mMinimalXULSheet);
  MEASURE(mNoFramesSheet);
  MEASURE(mNoScriptSheet);
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
  LoadSheetURL("chrome://global/content/minimal-xul.css",
               mMinimalXULSheet, true);
  LoadSheetURL("resource://gre-resources/quirk.css",
               mQuirkSheet, true);
  LoadSheetURL("resource://gre/res/svg.css",
               mSVGSheet, true);
  LoadSheetURL("chrome://global/content/xul.css",
               mXULSheet, true);

  // The remaining sheets are created on-demand do to their use being rarer
  // (which helps save memory for Firefox OS apps) or because they need to
  // be re-loadable in DependentPrefChanged.
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

  // For each pref that controls a CSS feature that a UA style sheet depends
  // on (such as a pref that enables a property that a UA style sheet uses),
  // register DependentPrefChanged as a callback to ensure that the relevant
  // style sheets will be re-parsed.
  Preferences::RegisterCallback(&DependentPrefChanged,
                                "layout.css.ruby.enabled");
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

/* static */ void
nsLayoutStylesheetCache::InvalidateSheet(nsRefPtr<CSSStyleSheet>& aSheet)
{
  MOZ_ASSERT(gCSSLoader, "pref changed before we loaded a sheet?");

  if (aSheet) {
    gCSSLoader->ObsoleteSheet(aSheet->GetSheetURI());
    aSheet = nullptr;
  }
}

/* static */ void
nsLayoutStylesheetCache::DependentPrefChanged(const char* aPref, void* aData)
{
  MOZ_ASSERT(gStyleCache, "pref changed after shutdown?");

  // Cause any UA style sheets whose parsing depends on the value of prefs
  // to be re-parsed by dropping the sheet from gCSSLoader's cache then
  // setting our cached sheet pointer to null.  This will only work for sheets
  // that are loaded lazily.

  // for layout.css.ruby.enabled
  InvalidateSheet(gStyleCache->mUASheet);
  InvalidateSheet(gStyleCache->mHTMLSheet);
}

/* static */ void
nsLayoutStylesheetCache::InvalidatePreferenceSheets()
{
  if (!gStyleCache) {
    return;
  }

  gStyleCache->mContentPreferenceSheet = nullptr;
  gStyleCache->mChromePreferenceSheet = nullptr;
}

/* static */ void
nsLayoutStylesheetCache::AppendPreferenceRule(CSSStyleSheet* aSheet,
                                              const nsAString& aString)
{
  uint32_t result;
  aSheet->InsertRuleInternal(aString, aSheet->StyleRuleCount(), &result);
}

/* static */ void
nsLayoutStylesheetCache::AppendPreferenceColorRule(CSSStyleSheet* aSheet,
                                                   const char* aString,
                                                   nscolor aColor)
{
  nsAutoString rule;
  rule.AppendPrintf(
      aString, NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor));
  AppendPreferenceRule(aSheet, rule);
}

void
nsLayoutStylesheetCache::BuildPreferenceSheet(nsRefPtr<CSSStyleSheet>& aSheet,
                                              nsPresContext* aPresContext)
{
  aSheet = new CSSStyleSheet(CORS_NONE, mozilla::net::RP_Default);

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "about:PreferenceStyleSheet", nullptr);
  MOZ_ASSERT(uri, "URI creation shouldn't fail");

  aSheet->SetURIs(uri, uri, uri);
  aSheet->SetComplete();

  AppendPreferenceRule(aSheet,
      NS_LITERAL_STRING("@namespace url(http://www.w3.org/1999/xhtml);"));
  AppendPreferenceRule(aSheet,
      NS_LITERAL_STRING("@namespace svg url(http://www.w3.org/2000/svg);"));

  // Rules for link styling.

  AppendPreferenceColorRule(aSheet,
      "*|*:link { color: #%02x%02x%02x; }",
      aPresContext->DefaultLinkColor());
  AppendPreferenceColorRule(aSheet,
      "*|*:-moz-any-link:active { color: #%02x%02x%02x; }",
      aPresContext->DefaultActiveLinkColor());
  AppendPreferenceColorRule(aSheet,
      "*|*:visited { color: #%02x%02x%02x; }",
      aPresContext->DefaultVisitedLinkColor());

  AppendPreferenceRule(aSheet,
      aPresContext->GetCachedBoolPref(kPresContext_UnderlineLinks) ?
        NS_LITERAL_STRING(
            "*|*:-moz-any-link:not(svg|a) { text-decoration: underline; }") :
        NS_LITERAL_STRING(
            "*|*:-moz-any-link{ text-decoration: none; }"));

  // Rules for focus styling.

  bool focusRingOnAnything = aPresContext->GetFocusRingOnAnything();
  uint8_t focusRingWidth = aPresContext->FocusRingWidth();
  uint8_t focusRingStyle = aPresContext->GetFocusRingStyle();

  if ((focusRingWidth != 1 && focusRingWidth <= 4) || focusRingOnAnything) {
    if (focusRingWidth != 1) {
      // If the focus ring width is different from the default, fix buttons
      // with rings.
      nsString rule;
      rule.AppendPrintf(
          "button::-moz-focus-inner, input[type=\"reset\"]::-moz-focus-inner, "
          "input[type=\"button\"]::-moz-focus-inner, "
          "input[type=\"submit\"]::-moz-focus-inner { "
          "padding: 1px 2px 1px 2px; "
          "border: %d %s transparent !important; }",
          focusRingWidth,
          focusRingWidth == 0 ? (const char*) "solid" : (const char*) "dotted");
      AppendPreferenceRule(aSheet, rule);

      // NS_LITERAL_STRING doesn't work with concatenated string literals, hence
      // the newline escaping.
      AppendPreferenceRule(aSheet, NS_LITERAL_STRING("\
button:focus::-moz-focus-inner, \
input[type=\"reset\"]:focus::-moz-focus-inner, \
input[type=\"button\"]:focus::-moz-focus-inner, \
input[type=\"submit\"]:focus::-moz-focus-inner { \
border-color: ButtonText !important; }"));
    }

    nsString rule;
    if (focusRingOnAnything) {
      rule.AppendLiteral(":focus");
    } else {
      rule.AppendLiteral("*|*:link:focus, *|*:visited:focus");
    }
    rule.AppendPrintf(" { outline: %dpx ", focusRingWidth);
    if (focusRingStyle == 0) { // solid
      rule.AppendLiteral("solid -moz-mac-focusring !important; "
                         "-moz-outline-radius: 3px; outline-offset: 1px; }");
    } else {
      rule.AppendLiteral("dotted WindowText !important; }");
    }
    AppendPreferenceRule(aSheet, rule);
  }

  if (aPresContext->GetUseFocusColors()) {
    nsString rule;
    nscolor focusText = aPresContext->FocusTextColor();
    nscolor focusBG = aPresContext->FocusBackgroundColor();
    rule.AppendPrintf(
        "*:focus, *:focus > font { color: #%02x%02x%02x !important; "
        "background-color: #%02x%02x%02x !important; }",
        NS_GET_R(focusText), NS_GET_G(focusText), NS_GET_B(focusText),
        NS_GET_R(focusBG), NS_GET_G(focusBG), NS_GET_B(focusBG));
    AppendPreferenceRule(aSheet, rule);
  }
}

mozilla::StaticRefPtr<nsLayoutStylesheetCache>
nsLayoutStylesheetCache::gStyleCache;

mozilla::css::Loader*
nsLayoutStylesheetCache::gCSSLoader = nullptr;
