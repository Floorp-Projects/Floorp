/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* diagnostic reporting for CSS style sheet parser */

#include "mozilla/css/ErrorReporter.h"

#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Loader.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/SystemGroup.h"
#include "nsCSSScanner.h"
#include "nsIConsoleService.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIFactory.h"
#include "nsINode.h"
#include "nsIScriptError.h"
#include "nsISensitiveInfoHiddenURI.h"
#include "nsIStringBundle.h"
#include "nsServiceManagerUtils.h"
#include "nsStyleUtil.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"

using namespace mozilla;
using namespace mozilla::css;

namespace {
class ShortTermURISpecCache : public Runnable {
public:
  ShortTermURISpecCache()
   : Runnable("ShortTermURISpecCache")
   , mPending(false) {}

  nsString const& GetSpec(nsIURI* aURI) {
    if (mURI != aURI) {
      mURI = aURI;

      if (NS_FAILED(NS_GetSanitizedURIStringFromURI(mURI, mSpec))) {
        mSpec.AssignLiteral("[nsIURI::GetSpec failed]");
      }
    }
    return mSpec;
  }

  bool IsInUse() const { return mURI != nullptr; }
  bool IsPending() const { return mPending; }
  void SetPending() { mPending = true; }

  // When invoked as a runnable, zap the cache.
  NS_IMETHOD Run() override {
    mURI = nullptr;
    mSpec.Truncate();
    mPending = false;
    return NS_OK;
  }

private:
  nsCOMPtr<nsIURI> mURI;
  nsString mSpec;
  bool mPending;
};

} // namespace

bool ErrorReporter::sReportErrors = false;
bool ErrorReporter::sInitialized = false;

static nsIConsoleService *sConsoleService;
static nsIFactory *sScriptErrorFactory;
static nsIStringBundle *sStringBundle;
static ShortTermURISpecCache *sSpecCache;

#define CSS_ERRORS_PREF "layout.css.report_errors"

void
ErrorReporter::InitGlobals()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sInitialized, "should not have been called");

  sInitialized = true;

  if (NS_FAILED(Preferences::AddBoolVarCache(&sReportErrors,
                                             CSS_ERRORS_PREF,
                                             true))) {
    return;
  }

  nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (!cs) {
    return;
  }

  nsCOMPtr<nsIFactory> sf = do_GetClassObject(NS_SCRIPTERROR_CONTRACTID);
  if (!sf) {
    return;
  }

  nsCOMPtr<nsIStringBundleService> sbs = services::GetStringBundleService();
  if (!sbs) {
    return;
  }

  nsCOMPtr<nsIStringBundle> sb;
  nsresult rv = sbs->CreateBundle("chrome://global/locale/css.properties",
                                  getter_AddRefs(sb));
  if (NS_FAILED(rv) || !sb) {
    return;
  }

  cs.forget(&sConsoleService);
  sf.forget(&sScriptErrorFactory);
  sb.forget(&sStringBundle);
}

namespace mozilla {
namespace css {

/* static */ void
ErrorReporter::ReleaseGlobals()
{
  NS_IF_RELEASE(sConsoleService);
  NS_IF_RELEASE(sScriptErrorFactory);
  NS_IF_RELEASE(sStringBundle);
  NS_IF_RELEASE(sSpecCache);
}

ErrorReporter::ErrorReporter(const StyleSheet* aSheet,
                             const Loader* aLoader,
                             nsIURI* aURI)
  : mSheet(aSheet)
  , mLoader(aLoader)
  , mURI(aURI)
  , mInnerWindowID(0)
  , mErrorLineNumber(0)
  , mPrevErrorLineNumber(0)
  , mErrorColNumber(0)
{
}

ErrorReporter::~ErrorReporter()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Schedule deferred cleanup for cached data. We want to strike a
  // balance between performance and memory usage, so we only allow
  // short-term caching.
  if (sSpecCache && sSpecCache->IsInUse() && !sSpecCache->IsPending()) {
    nsCOMPtr<nsIRunnable> runnable(sSpecCache);
    nsresult rv =
      SystemGroup::Dispatch(TaskCategory::Other, runnable.forget());
    if (NS_FAILED(rv)) {
      // Peform the "deferred" cleanup immediately if the dispatch fails.
      sSpecCache->Run();
    } else {
      sSpecCache->SetPending();
    }
  }
}

bool
ErrorReporter::ShouldReportErrors(const nsIDocument& aDoc)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsIDocShell* shell = aDoc.GetDocShell();
  if (!shell) {
    return false;
  }

  bool report = false;
  shell->GetCssErrorReportingEnabled(&report);
  return report;
}

static nsINode*
SheetOwner(const StyleSheet& aSheet)
{
  if (nsINode* owner = aSheet.GetOwnerNode()) {
    return owner;
  }

  auto* associated = aSheet.GetAssociatedDocumentOrShadowRoot();
  return associated ? &associated->AsNode() : nullptr;
}

bool
ErrorReporter::ShouldReportErrors()
{
  MOZ_ASSERT(NS_IsMainThread());

  EnsureGlobalsInitialized();
  if (!sReportErrors) {
    return false;
  }

  if (mInnerWindowID) {
    // We already reported an error, and that has cleared mSheet and mLoader, so
    // we'd get the bogus value otherwise.
    return true;
  }

  if (mSheet) {
    nsINode* owner = SheetOwner(*mSheet);
    if (owner && ShouldReportErrors(*owner->OwnerDoc())) {
      return true;
    }
  }

  if (mLoader && mLoader->GetDocument() &&
      ShouldReportErrors(*mLoader->GetDocument())) {
    return true;
  }

  return false;
}

void
ErrorReporter::OutputError()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(ShouldReportErrors());

  if (mError.IsEmpty()) {
    return;
  }

  if (mInnerWindowID == 0 && (mSheet || mLoader)) {
    if (mSheet) {
      mInnerWindowID = mSheet->FindOwningWindowInnerID();
    }
    if (mInnerWindowID == 0 && mLoader) {
      nsIDocument* doc = mLoader->GetDocument();
      if (doc) {
        mInnerWindowID = doc->InnerWindowID();
      }
    }
    // don't attempt this again, even if we failed
    mSheet = nullptr;
    mLoader = nullptr;
  }

  if (mFileName.IsEmpty()) {
    if (mURI) {
      if (!sSpecCache) {
        sSpecCache = new ShortTermURISpecCache;
        NS_ADDREF(sSpecCache);
      }
      mFileName = sSpecCache->GetSpec(mURI);
      mURI = nullptr;
    } else {
      mFileName.AssignLiteral("from DOM");
    }
  }

  nsresult rv;
  nsCOMPtr<nsIScriptError> errorObject =
    do_CreateInstance(sScriptErrorFactory, &rv);

  if (NS_SUCCEEDED(rv)) {
    // It is safe to used InitWithSanitizedSource because mFileName is
    // an already anonymized uri spec.
    rv = errorObject->InitWithSanitizedSource(mError,
                                              mFileName,
                                              mErrorLine,
                                              mErrorLineNumber,
                                              mErrorColNumber,
                                              nsIScriptError::warningFlag,
                                              "CSS Parser",
                                              mInnerWindowID);
    if (NS_SUCCEEDED(rv)) {
      sConsoleService->LogMessage(errorObject);
    }
  }

  ClearError();
}

// When Stylo's CSS parser is in use, this reporter does not have access to the CSS parser's
// state. The users of ErrorReporter need to provide:
// - the line number of the error
// - the column number of the error
// - the complete source line containing the invalid CSS

void
ErrorReporter::OutputError(uint32_t aLineNumber,
                           uint32_t aColNumber,
                           const nsACString& aSourceLine)
{
  mErrorLineNumber = aLineNumber;
  mErrorColNumber = aColNumber;

  // Retrieve the error line once per line, and reuse the same nsString
  // for all errors on that line.  That causes the text of the line to
  // be shared among all the nsIScriptError objects.
  if (mErrorLine.IsEmpty() || mErrorLineNumber != mPrevErrorLineNumber) {
    mErrorLine.Truncate();
    // This could be a really long string for minified CSS; just leave it empty if we OOM.
    if (!AppendUTF8toUTF16(aSourceLine, mErrorLine, fallible)) {
      mErrorLine.Truncate();
    }

    mPrevErrorLineNumber = aLineNumber;
  }

  OutputError();
}

void
ErrorReporter::ClearError()
{
  mError.Truncate();
}

void
ErrorReporter::AddToError(const nsString &aErrorText)
{
  MOZ_ASSERT(ShouldReportErrors());

  if (mError.IsEmpty()) {
    mError = aErrorText;
  } else {
    mError.AppendLiteral("  ");
    mError.Append(aErrorText);
  }
}

void
ErrorReporter::ReportUnexpected(const char *aMessage)
{
  MOZ_ASSERT(ShouldReportErrors());

  nsAutoString str;
  sStringBundle->GetStringFromName(aMessage, str);
  AddToError(str);
}

void
ErrorReporter::ReportUnexpectedUnescaped(const char *aMessage,
                                         const nsAutoString& aParam)
{
  MOZ_ASSERT(ShouldReportErrors());

  const char16_t *params[1] = { aParam.get() };

  nsAutoString str;
  sStringBundle->FormatStringFromName(aMessage, params, ArrayLength(params),
                                      str);
  AddToError(str);
}

} // namespace css
} // namespace mozilla
