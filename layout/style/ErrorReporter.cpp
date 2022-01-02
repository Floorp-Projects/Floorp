/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* diagnostic reporting for CSS style sheet parser */

#include "mozilla/css/ErrorReporter.h"

#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Loader.h"
#include "mozilla/Preferences.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Components.h"
#include "nsIConsoleService.h"
#include "mozilla/dom/Document.h"
#include "nsComponentManagerUtils.h"
#include "nsIDocShell.h"
#include "nsIFactory.h"
#include "nsINode.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"
#include "nsServiceManagerUtils.h"
#include "nsStyleUtil.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

namespace {
class ShortTermURISpecCache : public Runnable {
 public:
  ShortTermURISpecCache()
      : Runnable("ShortTermURISpecCache"), mPending(false) {}

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

}  // namespace

bool ErrorReporter::sInitialized = false;

static nsIConsoleService* sConsoleService;
static nsIFactory* sScriptErrorFactory;
static nsIStringBundle* sStringBundle;
static ShortTermURISpecCache* sSpecCache;

void ErrorReporter::InitGlobals() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sInitialized, "should not have been called");

  sInitialized = true;

  nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (!cs) {
    return;
  }

  nsCOMPtr<nsIFactory> sf = do_GetClassObject(NS_SCRIPTERROR_CONTRACTID);
  if (!sf) {
    return;
  }

  nsCOMPtr<nsIStringBundleService> sbs = components::StringBundle::Service();
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

/* static */
void ErrorReporter::ReleaseGlobals() {
  NS_IF_RELEASE(sConsoleService);
  NS_IF_RELEASE(sScriptErrorFactory);
  NS_IF_RELEASE(sStringBundle);
  NS_IF_RELEASE(sSpecCache);
}

uint64_t ErrorReporter::FindInnerWindowId(const StyleSheet* aSheet,
                                          const Loader* aLoader) {
  if (aSheet) {
    if (uint64_t id = aSheet->FindOwningWindowInnerID()) {
      return id;
    }
  }
  if (aLoader) {
    if (Document* doc = aLoader->GetDocument()) {
      return doc->InnerWindowID();
    }
  }
  return 0;
}

ErrorReporter::ErrorReporter(uint64_t aInnerWindowId)
    : mInnerWindowId(aInnerWindowId) {
  EnsureGlobalsInitialized();
}

ErrorReporter::~ErrorReporter() {
  MOZ_ASSERT(NS_IsMainThread());
  // Schedule deferred cleanup for cached data. We want to strike a
  // balance between performance and memory usage, so we only allow
  // short-term caching.
  if (sSpecCache && sSpecCache->IsInUse() && !sSpecCache->IsPending()) {
    nsCOMPtr<nsIRunnable> runnable(sSpecCache);
    nsresult rv =
        SchedulerGroup::Dispatch(TaskCategory::Other, runnable.forget());
    if (NS_FAILED(rv)) {
      // Peform the "deferred" cleanup immediately if the dispatch fails.
      sSpecCache->Run();
    } else {
      sSpecCache->SetPending();
    }
  }
}

bool ErrorReporter::ShouldReportErrors(const Document& aDoc) {
  MOZ_ASSERT(NS_IsMainThread());
  nsIDocShell* shell = aDoc.GetDocShell();
  if (!shell) {
    return false;
  }

  bool report = false;
  shell->GetCssErrorReportingEnabled(&report);
  return report;
}

static nsINode* SheetOwner(const StyleSheet& aSheet) {
  if (nsINode* owner = aSheet.GetOwnerNode()) {
    return owner;
  }

  auto* associated = aSheet.GetAssociatedDocumentOrShadowRoot();
  return associated ? &associated->AsNode() : nullptr;
}

bool ErrorReporter::ShouldReportErrors(const StyleSheet* aSheet,
                                       const Loader* aLoader) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!StaticPrefs::layout_css_report_errors()) {
    return false;
  }

  if (aSheet) {
    nsINode* owner = SheetOwner(*aSheet);
    if (owner && ShouldReportErrors(*owner->OwnerDoc())) {
      return true;
    }
  }

  if (aLoader && aLoader->GetDocument() &&
      ShouldReportErrors(*aLoader->GetDocument())) {
    return true;
  }

  return false;
}

void ErrorReporter::OutputError(const nsACString& aSourceLine,
                                const nsACString& aSelectors,
                                uint32_t aLineNumber, uint32_t aColNumber,
                                nsIURI* aURI) {
  nsAutoString errorLine;
  // This could be a really long string for minified CSS; just leave it empty
  // if we OOM.
  if (!AppendUTF8toUTF16(aSourceLine, errorLine, fallible)) {
    errorLine.Truncate();
  }

  nsAutoString selectors;
  if (!AppendUTF8toUTF16(aSelectors, selectors, fallible)) {
    selectors.Truncate();
  }

  if (mError.IsEmpty()) {
    return;
  }

  nsAutoString fileName;
  if (aURI) {
    if (!sSpecCache) {
      sSpecCache = new ShortTermURISpecCache;
      NS_ADDREF(sSpecCache);
    }
    fileName = sSpecCache->GetSpec(aURI);
  } else {
    fileName.AssignLiteral("from DOM");
  }

  nsresult rv;
  nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(sScriptErrorFactory, &rv);

  if (NS_SUCCEEDED(rv)) {
    // It is safe to used InitWithSanitizedSource because fileName is
    // an already anonymized uri spec.
    rv = errorObject->InitWithSanitizedSource(
        mError, fileName, errorLine, aLineNumber, aColNumber,
        nsIScriptError::warningFlag, "CSS Parser", mInnerWindowId);

    if (NS_SUCCEEDED(rv)) {
      errorObject->SetCssSelectors(selectors);
      sConsoleService->LogMessage(errorObject);
    }
  }

  mError.Truncate();
}

void ErrorReporter::AddToError(const nsString& aErrorText) {
  if (mError.IsEmpty()) {
    mError = aErrorText;
  } else {
    mError.AppendLiteral("  ");
    mError.Append(aErrorText);
  }
}

void ErrorReporter::ReportUnexpected(const char* aMessage) {
  nsAutoString str;
  sStringBundle->GetStringFromName(aMessage, str);
  AddToError(str);
}

void ErrorReporter::ReportUnexpectedUnescaped(
    const char* aMessage, const nsTArray<nsString>& aParam) {
  nsAutoString str;
  sStringBundle->FormatStringFromName(aMessage, aParam, str);
  AddToError(str);
}

}  // namespace css
}  // namespace mozilla
