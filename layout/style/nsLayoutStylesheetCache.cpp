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

// Includes for the crash report annotation in ErrorLoadingBuiltinSheet.
#ifdef MOZ_CRASHREPORTER
#include "mozilla/Omnijar.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsExceptionHandler.h"
#include "nsIChromeRegistry.h"
#include "nsISimpleEnumerator.h"
#include "nsISubstitutingProtocolHandler.h"
#include "zlib.h"
#include "nsZipArchive.h"
#endif

using namespace mozilla;
using namespace mozilla::css;

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
                 gStyleCache->mScrollbarsSheet, eAuthorSheetFeatures);
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
                 gStyleCache->mFormsSheet, eAgentSheetFeatures);
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
                 gStyleCache->mNumberControlSheet, eAgentSheetFeatures);
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
                 gStyleCache->mUASheet, eAgentSheetFeatures);
  }

  return gStyleCache->mUASheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::HTMLSheet()
{
  EnsureGlobal();

  if (!gStyleCache->mHTMLSheet) {
    LoadSheetURL("resource://gre-resources/html.css",
                 gStyleCache->mHTMLSheet, eAgentSheetFeatures);
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
                 gStyleCache->mMathMLSheet, eAgentSheetFeatures);
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
    // If you update the data: URL, also update noscript.css  (See bug 1194856.)
    LoadSheetURL(
#ifdef RELEASE_BUILD
                 "data:text/css,noscript { display%3A none !important%3B }",
#else
                 "resource://gre-resources/noscript.css",
#endif
                 gStyleCache->mNoScriptSheet, eAgentSheetFeatures);
  }

  return gStyleCache->mNoScriptSheet;
}

CSSStyleSheet*
nsLayoutStylesheetCache::NoFramesSheet()
{
  EnsureGlobal();

  if (!gStyleCache->mNoFramesSheet) {
    // If you update the data: URL, also update noframes.css  (See bug 1194856.)
    LoadSheetURL(
#ifdef RELEASE_BUILD
                 "data:text/css,noframes { display%3A block%3B } "
                 "frame%2C frameset%2C iframe { display%3A none !important%3B }",
#else
                 "resource://gre-resources/noframes.css",
#endif
                 gStyleCache->mNoFramesSheet, eAgentSheetFeatures);
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
                 gStyleCache->mContentEditableSheet, eAgentSheetFeatures);
  }

  return gStyleCache->mContentEditableSheet;
}

/* static */ CSSStyleSheet*
nsLayoutStylesheetCache::DesignModeSheet()
{
  EnsureGlobal();

  if (!gStyleCache->mDesignModeSheet) {
    LoadSheetURL("resource://gre/res/designmode.css",
                 gStyleCache->mDesignModeSheet, eAgentSheetFeatures);
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
               mCounterStylesSheet, eAgentSheetFeatures);
  LoadSheetURL("chrome://global/content/minimal-xul.css",
               mMinimalXULSheet, eAgentSheetFeatures);
  LoadSheetURL("resource://gre-resources/quirk.css",
               mQuirkSheet, eAgentSheetFeatures);
  LoadSheetURL("resource://gre/res/svg.css",
               mSVGSheet, eAgentSheetFeatures);
  LoadSheetURL("chrome://global/content/xul.css",
               mXULSheet, eAgentSheetFeatures);

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

  LoadSheetFile(contentFile, mUserContentSheet, eUserSheetFeatures);
  LoadSheetFile(chromeFile, mUserChromeSheet, eUserSheetFeatures);
}

/* static */ void
nsLayoutStylesheetCache::LoadSheetURL(const char* aURL,
                                      RefPtr<CSSStyleSheet>& aSheet,
                                      SheetParsingMode aParsingMode)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), aURL);
  LoadSheet(uri, aSheet, aParsingMode);
  if (!aSheet) {
    NS_ERROR(nsPrintfCString("Could not load %s", aURL).get());
  }
}

void
nsLayoutStylesheetCache::LoadSheetFile(nsIFile* aFile,
                                       RefPtr<CSSStyleSheet>& aSheet,
                                       SheetParsingMode aParsingMode)
{
  bool exists = false;
  aFile->Exists(&exists);

  if (!exists) return;

  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), aFile);

  LoadSheet(uri, aSheet, aParsingMode);
}

#ifdef MOZ_CRASHREPORTER
static inline nsresult
ComputeCRC32(nsIFile* aFile, uint32_t* aResult)
{
  PRFileDesc* fd;
  nsresult rv = aFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t crc = crc32(0, nullptr, 0);

  unsigned char buf[512];
  int32_t n;
  while ((n = PR_Read(fd, buf, sizeof(buf))) > 0) {
    crc = crc32(crc, buf, n);
  }
  PR_Close(fd);

  if (n < 0) {
    return NS_ERROR_FAILURE;
  }

  *aResult = crc;
  return NS_OK;
}

static void
ListInterestingFiles(nsString& aAnnotation, nsIFile* aFile,
                     const nsTArray<nsString>& aInterestingFilenames)
{
  nsString filename;
  aFile->GetLeafName(filename);
  for (const nsString& interestingFilename : aInterestingFilenames) {
    if (interestingFilename == filename) {
      nsString path;
      aFile->GetPath(path);
      aAnnotation.AppendLiteral("  ");
      aAnnotation.Append(path);
      aAnnotation.AppendLiteral(" (");
      int64_t size;
      if (NS_SUCCEEDED(aFile->GetFileSize(&size))) {
        aAnnotation.AppendPrintf("%ld", size);
      } else {
        aAnnotation.AppendLiteral("???");
      }
      aAnnotation.AppendLiteral(" bytes, crc32 = ");
      uint32_t crc;
      nsresult rv = ComputeCRC32(aFile, &crc);
      if (NS_SUCCEEDED(rv)) {
        aAnnotation.AppendPrintf("0x%08x)\n", crc);
      } else {
        aAnnotation.AppendPrintf("error 0x%08x)\n", uint32_t(rv));
      }
      return;
    }
  }

  bool isDir = false;
  aFile->IsDirectory(&isDir);

  if (!isDir) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> entries;
  if (NS_FAILED(aFile->GetDirectoryEntries(getter_AddRefs(entries)))) {
    aAnnotation.AppendLiteral("  (failed to enumerated directory)\n");
    return;
  }

  for (;;) {
    bool hasMore = false;
    if (NS_FAILED(entries->HasMoreElements(&hasMore))) {
      aAnnotation.AppendLiteral("  (failed during directory enumeration)\n");
      return;
    }
    if (!hasMore) {
      break;
    }

    nsCOMPtr<nsISupports> entry;
    if (NS_FAILED(entries->GetNext(getter_AddRefs(entry)))) {
      aAnnotation.AppendLiteral("  (failed during directory enumeration)\n");
      return;
    }

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    if (file) {
      ListInterestingFiles(aAnnotation, file, aInterestingFilenames);
    }
  }
}

// Generate a crash report annotation to help debug issues with style
// sheets failing to load (bug 1194856).
static void
AnnotateCrashReport(nsIURI* aURI)
{
  nsAutoCString spec;
  nsAutoCString scheme;
  nsDependentCSubstring filename;
  if (aURI) {
    aURI->GetSpec(spec);
    aURI->GetScheme(scheme);
    int32_t i = spec.RFindChar('/');
    if (i != -1) {
      filename.Rebind(spec, i + 1);
    }
  }

  nsString annotation;

  // The URL of the sheet that failed to load.
  annotation.AppendLiteral("Error loading sheet: ");
  annotation.Append(NS_ConvertUTF8toUTF16(spec).get());
  annotation.Append('\n');

  annotation.AppendLiteral("NS_ERROR_FILE_CORRUPTION reason: ");
  if (nsZipArchive::sFileCorruptedReason) {
    annotation.Append(NS_ConvertUTF8toUTF16(nsZipArchive::sFileCorruptedReason).get());
    annotation.Append('\n');
  } else {
    annotation.AppendLiteral("(none)\n");
  }

  // The jar: or file: URL that the sheet's resource: or chrome: URL
  // resolves to.
  if (scheme.EqualsLiteral("resource")) {
    annotation.AppendLiteral("Real location: ");
    nsCOMPtr<nsISubstitutingProtocolHandler> handler;
    nsCOMPtr<nsIIOService> io(do_GetIOService());
    if (io) {
      nsCOMPtr<nsIProtocolHandler> ph;
      io->GetProtocolHandler(scheme.get(), getter_AddRefs(ph));
      if (ph) {
        handler = do_QueryInterface(ph);
      }
    }
    if (!handler) {
      annotation.AppendLiteral("(ResolveURI failed)\n");
    } else {
      nsAutoCString resolvedSpec;
      handler->ResolveURI(aURI, resolvedSpec);
      annotation.Append(NS_ConvertUTF8toUTF16(resolvedSpec));
      annotation.Append('\n');
    }
  } else if (scheme.EqualsLiteral("chrome")) {
    annotation.AppendLiteral("Real location: ");
    nsCOMPtr<nsIChromeRegistry> reg =
      mozilla::services::GetChromeRegistryService();
    if (!reg) {
      annotation.AppendLiteral("(no chrome registry)\n");
    } else {
      nsCOMPtr<nsIURI> resolvedURI;
      reg->ConvertChromeURL(aURI, getter_AddRefs(resolvedURI));
      if (!resolvedURI) {
        annotation.AppendLiteral("(ConvertChromeURL failed)\n");
      } else {
        nsAutoCString resolvedSpec;
        resolvedURI->GetSpec(resolvedSpec);
        annotation.Append(NS_ConvertUTF8toUTF16(resolvedSpec));
        annotation.Append('\n');
      }
    }
  }

  nsTArray<nsString> interestingFiles;
  interestingFiles.AppendElement(NS_LITERAL_STRING("chrome.manifest"));
  interestingFiles.AppendElement(NS_LITERAL_STRING("omni.ja"));
  interestingFiles.AppendElement(NS_ConvertUTF8toUTF16(filename));

  annotation.AppendLiteral("GRE directory: ");
  nsCOMPtr<nsIFile> file;
  nsDirectoryService::gService->Get(NS_GRE_DIR, NS_GET_IID(nsIFile),
                                    getter_AddRefs(file));
  if (file) {
    // The Firefox installation directory.
    nsString path;
    file->GetPath(path);
    annotation.Append(path);
    annotation.Append('\n');

    // List interesting files -- any chrome.manifest or omni.ja file or any file
    // whose name is the sheet's filename -- under the Firefox installation
    // directory.
    annotation.AppendLiteral("Interesting files in the GRE directory:\n");
    ListInterestingFiles(annotation, file, interestingFiles);

    // If the Firefox installation directory has a chrome.manifest file, let's
    // see what's in it.
    file->Append(NS_LITERAL_STRING("chrome.manifest"));
    bool exists = false;
    file->Exists(&exists);
    if (exists) {
      annotation.AppendLiteral("Contents of chrome.manifest:\n[[[\n");
      PRFileDesc* fd;
      if (NS_SUCCEEDED(file->OpenNSPRFileDesc(PR_RDONLY, 0, &fd))) {
        nsCString contents;
        char buf[512];
        int32_t n;
        while ((n = PR_Read(fd, buf, sizeof(buf))) > 0) {
          contents.Append(buf, n);
        }
        if (n < 0) {
          annotation.AppendLiteral("  (error while reading)\n");
        } else {
          annotation.Append(NS_ConvertUTF8toUTF16(contents));
        }
        PR_Close(fd);
      }
      annotation.AppendLiteral("]]]\n");
    }
  } else {
    annotation.AppendLiteral("(none)\n");
  }

  // The jar: or file: URL prefix that chrome: and resource: URLs get translated
  // to.
  annotation.AppendLiteral("GRE omnijar URI string: ");
  nsCString uri;
  nsresult rv = Omnijar::GetURIString(Omnijar::GRE, uri);
  if (NS_FAILED(rv)) {
    annotation.AppendLiteral("(failed)\n");
  } else {
    annotation.Append(NS_ConvertUTF8toUTF16(uri));
    annotation.Append('\n');
  }

  RefPtr<nsZipArchive> zip = Omnijar::GetReader(Omnijar::GRE);
  if (zip) {
    // List interesting files in the GRE omnijar.
    annotation.AppendLiteral("Interesting files in the GRE omnijar:\n");
    nsZipFind* find;
    rv = zip->FindInit(nullptr, &find);
    if (NS_FAILED(rv)) {
      annotation.AppendPrintf("  (FindInit failed with 0x%08x)\n", rv);
    } else if (!find) {
      annotation.AppendLiteral("  (FindInit returned null)\n");
    } else {
      const char* result;
      uint16_t len;
      while (NS_SUCCEEDED(find->FindNext(&result, &len))) {
        nsCString itemPathname;
        nsString itemFilename;
        itemPathname.Append(result, len);
        int32_t i = itemPathname.RFindChar('/');
        if (i != -1) {
          itemFilename = NS_ConvertUTF8toUTF16(Substring(itemPathname, i + 1));
        }
        for (const nsString& interestingFile : interestingFiles) {
          if (interestingFile == itemFilename) {
            annotation.AppendLiteral("  ");
            annotation.Append(NS_ConvertUTF8toUTF16(itemPathname));
            nsZipItem* item = zip->GetItem(itemPathname.get());
            if (!item) {
              annotation.AppendLiteral(" (GetItem failed)\n");
            } else {
              annotation.AppendPrintf(" (%d bytes, crc32 = 0x%08x)\n",
                                      item->RealSize(),
                                      item->CRC32());
            }
            break;
          }
        }
      }
      delete find;
    }
  } else {
    annotation.AppendLiteral("No GRE omnijar\n");
  }

  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("SheetLoadFailure"),
                                     NS_ConvertUTF16toUTF8(annotation));
}
#endif

static void
ErrorLoadingBuiltinSheet(nsIURI* aURI, const char* aMsg)
{
#ifdef MOZ_CRASHREPORTER
  AnnotateCrashReport(aURI);
#endif

  nsAutoCString spec;
  if (aURI) {
    aURI->GetSpec(spec);
  }
  NS_RUNTIMEABORT(nsPrintfCString("%s loading built-in stylesheet '%s'",
                                  aMsg, spec.get()).get());
}

void
nsLayoutStylesheetCache::LoadSheet(nsIURI* aURI,
                                   RefPtr<CSSStyleSheet>& aSheet,
                                   SheetParsingMode aParsingMode)
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


#ifdef MOZ_CRASHREPORTER
  nsZipArchive::sFileCorruptedReason = nullptr;
#endif
  nsresult rv = gCSSLoader->LoadSheetSync(aURI, aParsingMode, true,
                                          getter_AddRefs(aSheet));
  if (NS_FAILED(rv)) {
    ErrorLoadingBuiltinSheet(aURI,
      nsPrintfCString("LoadSheetSync failed with error %x", rv).get());
  }
}

/* static */ void
nsLayoutStylesheetCache::InvalidateSheet(RefPtr<CSSStyleSheet>& aSheet)
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
nsLayoutStylesheetCache::BuildPreferenceSheet(RefPtr<CSSStyleSheet>& aSheet,
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
