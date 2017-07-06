/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIFactory.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"
#include "nsServiceManagerUtils.h"
#include "nsStyleUtil.h"
#include "nsThreadUtils.h"

#ifdef CSS_REPORT_PARSE_ERRORS

using namespace mozilla;

namespace {
class ShortTermURISpecCache : public Runnable {
public:
  ShortTermURISpecCache()
   : Runnable("ShortTermURISpecCache")
   , mPending(false) {}

  nsString const& GetSpec(nsIURI* aURI) {
    if (mURI != aURI) {
      mURI = aURI;

      nsAutoCString cSpec;
      nsresult rv = mURI->GetSpec(cSpec);
      if (NS_FAILED(rv)) {
        cSpec.AssignLiteral("[nsIURI::GetSpec failed]");
      }
      CopyUTF8toUTF16(cSpec, mSpec);
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

static bool sReportErrors;
static nsIConsoleService *sConsoleService;
static nsIFactory *sScriptErrorFactory;
static nsIStringBundle *sStringBundle;
static ShortTermURISpecCache *sSpecCache;

#define CSS_ERRORS_PREF "layout.css.report_errors"

static bool
InitGlobals()
{
  MOZ_ASSERT(!sConsoleService && !sScriptErrorFactory && !sStringBundle,
             "should not have been called");

  if (NS_FAILED(Preferences::AddBoolVarCache(&sReportErrors, CSS_ERRORS_PREF,
                                             true))) {
    return false;
  }

  nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (!cs) {
    return false;
  }

  nsCOMPtr<nsIFactory> sf = do_GetClassObject(NS_SCRIPTERROR_CONTRACTID);
  if (!sf) {
    return false;
  }

  nsCOMPtr<nsIStringBundleService> sbs = services::GetStringBundleService();
  if (!sbs) {
    return false;
  }

  nsCOMPtr<nsIStringBundle> sb;
  nsresult rv = sbs->CreateBundle("chrome://global/locale/css.properties",
                                  getter_AddRefs(sb));
  if (NS_FAILED(rv) || !sb) {
    return false;
  }

  cs.forget(&sConsoleService);
  sf.forget(&sScriptErrorFactory);
  sb.forget(&sStringBundle);

  return true;
}

static inline bool
ShouldReportErrors()
{
  if (!sConsoleService) {
    if (!InitGlobals()) {
      return false;
    }
  }
  return sReportErrors;
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

ErrorReporter::ErrorReporter(const nsCSSScanner& aScanner,
                             const StyleSheet* aSheet,
                             const Loader* aLoader,
                             nsIURI* aURI)
  : mScanner(&aScanner), mSheet(aSheet), mLoader(aLoader), mURI(aURI),
    mInnerWindowID(0), mErrorLineNumber(0), mPrevErrorLineNumber(0),
    mErrorColNumber(0)
{
}

ErrorReporter::ErrorReporter(const StyleSheet* aSheet,
                             const Loader* aLoader)
  : mScanner(nullptr), mSheet(aSheet), mLoader(aLoader), mURI(nullptr),
    mInnerWindowID(0), mErrorLineNumber(0), mPrevErrorLineNumber(0),
    mErrorColNumber(0)
{
}

ErrorReporter::~ErrorReporter()
{
  // Schedule deferred cleanup for cached data. We want to strike a
  // balance between performance and memory usage, so we only allow
  // short-term caching.
  if (sSpecCache && sSpecCache->IsInUse() && !sSpecCache->IsPending()) {
    nsCOMPtr<nsIRunnable> runnable(sSpecCache);
    nsresult rv =
      SystemGroup::Dispatch("ShortTermURISpecCache", TaskCategory::Other,
                            runnable.forget());
    if (NS_FAILED(rv)) {
      // Peform the "deferred" cleanup immediately if the dispatch fails.
      sSpecCache->Run();
    } else {
      sSpecCache->SetPending();
    }
  }
}

void
ErrorReporter::OutputError()
{
  if (mError.IsEmpty()) {
    return;
  }
  if (!ShouldReportErrors()) {
    ClearError();
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
    rv = errorObject->InitWithWindowID(mError,
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

void
ErrorReporter::OutputError(uint32_t aLineNumber, uint32_t aColNumber)
{
  mErrorLineNumber = aLineNumber;
  mErrorColNumber = aColNumber;
  OutputError();
}

// When Stylo's CSS parser is in use, this reporter does not have access to the CSS parser's
// state. The users of ErrorReporter need to provide:
// - the line number of the error
// - the column number of the error
// - the URI that triggered the error
// - the complete source line containing the invalid CSS

void
ErrorReporter::OutputError(uint32_t aLineNumber,
                           uint32_t aColNumber,
                           nsIURI* aURI,
                           const nsACString& aSourceLine)
{
  DebugOnly<bool> equal = false;
  MOZ_ASSERT(!mURI || (NS_SUCCEEDED(mURI->Equals(aURI, &equal)) && equal));
  mURI = aURI;
  mErrorLine.Truncate();
  // This could be a really long string for minified CSS; just leave it empty if we OOM.
  if (!AppendUTF8toUTF16(aSourceLine, mErrorLine, fallible)) {
    mErrorLine.Truncate();
  }
  mPrevErrorLineNumber = aLineNumber;
  OutputError(aLineNumber, aColNumber);
}

void
ErrorReporter::ClearError()
{
  mError.Truncate();
}

void
ErrorReporter::AddToError(const nsString &aErrorText)
{
  if (!ShouldReportErrors()) return;

  if (mError.IsEmpty()) {
    mError = aErrorText;
    mErrorLineNumber = mScanner ? mScanner->GetLineNumber() : 0;
    mErrorColNumber = mScanner ? mScanner->GetColumnNumber() : 0;
    // Retrieve the error line once per line, and reuse the same nsString
    // for all errors on that line.  That causes the text of the line to
    // be shared among all the nsIScriptError objects.
    if (mErrorLine.IsEmpty() || mErrorLineNumber != mPrevErrorLineNumber) {
      // Be careful here: the error line might be really long and OOM
      // when we try to make a copy here.  If so, just leave it empty.
      if (!mScanner || !mErrorLine.Assign(mScanner->GetCurrentLine(), fallible)) {
        mErrorLine.Truncate();
      }
      mPrevErrorLineNumber = mErrorLineNumber;
    }
  } else {
    mError.AppendLiteral("  ");
    mError.Append(aErrorText);
  }
}

void
ErrorReporter::ReportUnexpected(const char *aMessage)
{
  if (!ShouldReportErrors()) return;

  nsAutoString str;
  sStringBundle->GetStringFromName(NS_ConvertASCIItoUTF16(aMessage).get(),
                                   getter_Copies(str));
  AddToError(str);
}

void
ErrorReporter::ReportUnexpected(const char *aMessage,
                                const nsString &aParam)
{
  if (!ShouldReportErrors()) return;

  nsAutoString qparam;
  nsStyleUtil::AppendEscapedCSSIdent(aParam, qparam);
  const char16_t *params[1] = { qparam.get() };

  nsAutoString str;
  sStringBundle->FormatStringFromName(NS_ConvertASCIItoUTF16(aMessage).get(),
                                      params, ArrayLength(params),
                                      getter_Copies(str));
  AddToError(str);
}

void
ErrorReporter::ReportUnexpectedUnescaped(const char *aMessage,
                                         const nsAutoString& aParam)
{
  if (!ShouldReportErrors()) return;

  const char16_t *params[1] = { aParam.get() };

  nsAutoString str;
  sStringBundle->FormatStringFromName(NS_ConvertASCIItoUTF16(aMessage).get(),
                                      params, ArrayLength(params),
                                      getter_Copies(str));
  AddToError(str);
}

void
ErrorReporter::ReportUnexpected(const char *aMessage,
                                const nsCSSToken &aToken)
{
  if (!ShouldReportErrors()) return;

  nsAutoString tokenString;
  aToken.AppendToString(tokenString);
  ReportUnexpectedUnescaped(aMessage, tokenString);
}

void
ErrorReporter::ReportUnexpected(const char *aMessage,
                                const nsCSSToken &aToken,
                                char16_t aChar)
{
  if (!ShouldReportErrors()) return;

  nsAutoString tokenString;
  aToken.AppendToString(tokenString);
  const char16_t charStr[2] = { aChar, 0 };
  const char16_t *params[2] = { tokenString.get(), charStr };

  nsAutoString str;
  sStringBundle->FormatStringFromName(NS_ConvertASCIItoUTF16(aMessage).get(),
                                      params, ArrayLength(params),
                                      getter_Copies(str));
  AddToError(str);
}

void
ErrorReporter::ReportUnexpected(const char *aMessage,
                                const nsString &aParam,
                                const nsString &aValue)
{
  if (!ShouldReportErrors()) return;

  nsAutoString qparam;
  nsStyleUtil::AppendEscapedCSSIdent(aParam, qparam);
  const char16_t *params[2] = { qparam.get(), aValue.get() };

  nsAutoString str;
  sStringBundle->FormatStringFromName(NS_ConvertASCIItoUTF16(aMessage).get(),
                                      params, ArrayLength(params),
                                      getter_Copies(str));
  AddToError(str);
}

void
ErrorReporter::ReportUnexpectedEOF(const char *aMessage)
{
  if (!ShouldReportErrors()) return;

  nsAutoString innerStr;
  sStringBundle->GetStringFromName(NS_ConvertASCIItoUTF16(aMessage).get(),
                                   getter_Copies(innerStr));
  const char16_t *params[1] = { innerStr.get() };

  nsAutoString str;
  sStringBundle->FormatStringFromName(u"PEUnexpEOF2",
                                      params, ArrayLength(params),
                                      getter_Copies(str));
  AddToError(str);
}

void
ErrorReporter::ReportUnexpectedEOF(char16_t aExpected)
{
  if (!ShouldReportErrors()) return;

  const char16_t expectedStr[] = {
    char16_t('\''), aExpected, char16_t('\''), char16_t(0)
  };
  const char16_t *params[1] = { expectedStr };

  nsAutoString str;
  sStringBundle->FormatStringFromName(u"PEUnexpEOF2",
                                      params, ArrayLength(params),
                                      getter_Copies(str));
  AddToError(str);
}

} // namespace css
} // namespace mozilla

#endif
