/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of interface for managing user and user-agent style sheets */

#include "nsStyleSheetService.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PreloadedStyleSheet.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/Unused.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
#include "nsISimpleEnumerator.h"
#include "nsNetUtil.h"
#include "nsIConsoleService.h"
#include "nsIObserverService.h"
#include "nsLayoutStatics.h"
#include "nsLayoutUtils.h"

using namespace mozilla;

nsStyleSheetService *nsStyleSheetService::gInstance = nullptr;

nsStyleSheetService::nsStyleSheetService()
{
  static_assert(0 == AGENT_SHEET && 1 == USER_SHEET && 2 == AUTHOR_SHEET,
                "Convention for Style Sheet");
  NS_ASSERTION(!gInstance, "Someone is using CreateInstance instead of GetService");
  gInstance = this;
  nsLayoutStatics::AddRef();
}

nsStyleSheetService::~nsStyleSheetService()
{
  UnregisterWeakMemoryReporter(this);

  gInstance = nullptr;
  nsLayoutStatics::Release();
}

NS_IMPL_ISUPPORTS(
  nsStyleSheetService, nsIStyleSheetService, nsIMemoryReporter)

void
nsStyleSheetService::RegisterFromEnumerator(nsICategoryManager  *aManager,
                                            const char          *aCategory,
                                            nsISimpleEnumerator *aEnumerator,
                                            uint32_t             aSheetType)
{
  if (!aEnumerator)
    return;

  bool hasMore;
  while (NS_SUCCEEDED(aEnumerator->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> element;
    if (NS_FAILED(aEnumerator->GetNext(getter_AddRefs(element))))
      break;

    nsCOMPtr<nsISupportsCString> icStr = do_QueryInterface(element);
    NS_ASSERTION(icStr,
                 "category manager entries must be nsISupportsCStrings");

    nsAutoCString name;
    icStr->GetData(name);

    nsCString spec;
    aManager->GetCategoryEntry(nsDependentCString(aCategory), name, spec);

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), spec);
    if (uri)
      LoadAndRegisterSheetInternal(uri, aSheetType);
  }
}

static bool
SheetHasURI(StyleSheet* aSheet, nsIURI* aSheetURI)
{
  MOZ_ASSERT(aSheetURI);

  bool result;
  nsIURI* uri = aSheet->GetSheetURI();
  return uri &&
         NS_SUCCEEDED(uri->Equals(aSheetURI, &result)) &&
         result;
}

int32_t
nsStyleSheetService::FindSheetByURI(uint32_t aSheetType, nsIURI* aSheetURI)
{
  SheetArray& sheets = mSheets[aSheetType];
  for (int32_t i = sheets.Length() - 1; i >= 0; i-- ) {
    if (SheetHasURI(sheets[i], aSheetURI)) {
      return i;
    }
  }

  return -1;
}

nsresult
nsStyleSheetService::Init()
{
  // If you make changes here, consider whether
  // SVGDocument::EnsureNonSVGUserAgentStyleSheetsLoaded should be updated too.

  // Child processes get their style sheets from the ContentParent.
  if (XRE_IsContentProcess()) {
    return NS_OK;
  }

  // Enumerate all of the style sheet URIs registered in the category
  // manager and load them.

  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);

  NS_ENSURE_TRUE(catMan, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsISimpleEnumerator> sheets;
  catMan->EnumerateCategory("agent-style-sheets", getter_AddRefs(sheets));
  RegisterFromEnumerator(catMan, "agent-style-sheets", sheets, AGENT_SHEET);

  catMan->EnumerateCategory("user-style-sheets", getter_AddRefs(sheets));
  RegisterFromEnumerator(catMan, "user-style-sheets", sheets, USER_SHEET);

  catMan->EnumerateCategory("author-style-sheets", getter_AddRefs(sheets));
  RegisterFromEnumerator(catMan, "author-style-sheets", sheets, AUTHOR_SHEET);

  RegisterWeakMemoryReporter(this);

  return NS_OK;
}

NS_IMETHODIMP
nsStyleSheetService::LoadAndRegisterSheet(nsIURI *aSheetURI,
                                          uint32_t aSheetType)
{
  // Warn developers if their stylesheet URL has a #ref at the end.
  // Stylesheet URIs don't benefit from having a #ref suffix -- and if the
  // sheet is a data URI, someone might've created this #ref by accident (and
  // truncated their data-URI stylesheet) by using an unescaped # character in
  // a #RRGGBB color or #foo() ID-selector in their data-URI representation.
  bool hasRef;
  nsresult rv = aSheetURI->GetHasRef(&hasRef);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aSheetURI && hasRef) {
    nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    NS_WARNING_ASSERTION(consoleService, "Failed to get console service!");
    if (consoleService) {
      const char16_t* message = u"nsStyleSheetService::LoadAndRegisterSheet: "
        u"URI contains unescaped hash character, which might be truncating "
        u"the sheet, if it's a data URI.";
      consoleService->LogStringMessage(message);
    }
  }

  rv = LoadAndRegisterSheetInternal(aSheetURI, aSheetType);
  if (NS_SUCCEEDED(rv)) {
    // Hold on to a copy of the registered PresShells.
    nsTArray<nsCOMPtr<nsIPresShell>> toNotify(mPresShells);
    for (nsIPresShell* presShell : toNotify) {
      StyleSheet* sheet = mSheets[aSheetType].LastElement();
      presShell->NotifyStyleSheetServiceSheetAdded(sheet, aSheetType);
    }

    if (XRE_IsParentProcess()) {
      nsTArray<dom::ContentParent*> children;
      dom::ContentParent::GetAll(children);

      if (children.IsEmpty()) {
        return rv;
      }

      mozilla::ipc::URIParams uri;
      SerializeURI(aSheetURI, uri);

      for (uint32_t i = 0; i < children.Length(); i++) {
        Unused << children[i]->SendLoadAndRegisterSheet(uri, aSheetType);
      }
    }
  }
  return rv;
}

static nsresult
LoadSheet(nsIURI* aURI,
          css::SheetParsingMode aParsingMode,
          RefPtr<StyleSheet>* aResult)
{
  RefPtr<css::Loader> loader = new css::Loader;
  return loader->LoadSheetSync(aURI, aParsingMode, true, aResult);
}

nsresult
nsStyleSheetService::LoadAndRegisterSheetInternal(nsIURI *aSheetURI,
                                                  uint32_t aSheetType)
{
  NS_ENSURE_ARG_POINTER(aSheetURI);

  css::SheetParsingMode parsingMode;
  switch (aSheetType) {
    case AGENT_SHEET:
      parsingMode = css::eAgentSheetFeatures;
      break;

    case USER_SHEET:
      parsingMode = css::eUserSheetFeatures;
      break;

    case AUTHOR_SHEET:
      parsingMode = css::eAuthorSheetFeatures;
      break;

    default:
      NS_WARNING("invalid sheet type argument");
      return NS_ERROR_INVALID_ARG;
  }


  RefPtr<StyleSheet> sheet;
  nsresult rv = LoadSheet(aSheetURI, parsingMode, &sheet);
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(sheet);
  mSheets[aSheetType].AppendElement(sheet);

  return NS_OK;
}

NS_IMETHODIMP
nsStyleSheetService::SheetRegistered(nsIURI *sheetURI,
                                     uint32_t aSheetType, bool *_retval)
{
  NS_ENSURE_ARG(aSheetType == AGENT_SHEET ||
                aSheetType == USER_SHEET ||
                aSheetType == AUTHOR_SHEET);
  NS_ENSURE_ARG_POINTER(sheetURI);
  MOZ_ASSERT(_retval, "Null out param");

  // Check to see if we have the sheet.
  *_retval = (FindSheetByURI(aSheetType, sheetURI) >= 0);

  return NS_OK;
}

static nsresult
GetParsingMode(uint32_t aSheetType, css::SheetParsingMode* aParsingMode)
{
  switch (aSheetType) {
    case nsStyleSheetService::AGENT_SHEET:
      *aParsingMode = css::eAgentSheetFeatures;
      return NS_OK;

    case nsStyleSheetService::USER_SHEET:
      *aParsingMode = css::eUserSheetFeatures;
      return NS_OK;

    case nsStyleSheetService::AUTHOR_SHEET:
      *aParsingMode = css::eAuthorSheetFeatures;
      return NS_OK;

    default:
      NS_WARNING("invalid sheet type argument");
      return NS_ERROR_INVALID_ARG;
  }
}

NS_IMETHODIMP
nsStyleSheetService::PreloadSheet(nsIURI* aSheetURI, uint32_t aSheetType,
                                  nsIPreloadedStyleSheet** aSheet)
{
  MOZ_ASSERT(aSheet, "Null out param");
  NS_ENSURE_ARG_POINTER(aSheetURI);

  css::SheetParsingMode parsingMode;
  nsresult rv = GetParsingMode(aSheetType, &parsingMode);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<PreloadedStyleSheet> sheet;
  rv = PreloadedStyleSheet::Create(aSheetURI, parsingMode,
                                   getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = sheet->Preload();
  NS_ENSURE_SUCCESS(rv, rv);

  sheet.forget(aSheet);
  return NS_OK;
}

NS_IMETHODIMP
nsStyleSheetService::PreloadSheetAsync(nsIURI* aSheetURI, uint32_t aSheetType,
                                       JSContext* aCx,
                                       JS::MutableHandleValue aRval)
{
  NS_ENSURE_ARG_POINTER(aSheetURI);

  css::SheetParsingMode parsingMode;
  nsresult rv = GetParsingMode(aSheetType, &parsingMode);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(aCx);
  NS_ENSURE_TRUE(global, NS_ERROR_UNEXPECTED);

  ErrorResult errv;
  RefPtr<dom::Promise> promise = dom::Promise::Create(global, errv);
  if (errv.Failed()) {
    return errv.StealNSResult();
  }

  RefPtr<PreloadedStyleSheet> sheet;
  rv = PreloadedStyleSheet::Create(aSheetURI, parsingMode,
                                   getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);

  sheet->PreloadAsync(WrapNotNull(promise));

  if (!ToJSValue(aCx, promise, aRval)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsStyleSheetService::UnregisterSheet(nsIURI *aSheetURI, uint32_t aSheetType)
{
  NS_ENSURE_ARG(aSheetType == AGENT_SHEET ||
                aSheetType == USER_SHEET ||
                aSheetType == AUTHOR_SHEET);
  NS_ENSURE_ARG_POINTER(aSheetURI);

  RefPtr<StyleSheet> sheet;
  int32_t foundIndex = FindSheetByURI(aSheetType, aSheetURI);
  if (foundIndex >= 0) {
    sheet = mSheets[aSheetType][foundIndex];
    mSheets[aSheetType].RemoveElementAt(foundIndex);
  }

  // Hold on to a copy of the registered PresShells.
  nsTArray<nsCOMPtr<nsIPresShell>> toNotify(mPresShells);
  for (nsIPresShell* presShell : toNotify) {
    if (presShell->StyleSet()) {
      if (sheet) {
        presShell->NotifyStyleSheetServiceSheetRemoved(sheet, aSheetType);
      }
    }
  }

  if (XRE_IsParentProcess()) {
    nsTArray<dom::ContentParent*> children;
    dom::ContentParent::GetAll(children);

    if (children.IsEmpty()) {
      return NS_OK;
    }

    mozilla::ipc::URIParams uri;
    SerializeURI(aSheetURI, uri);

    for (uint32_t i = 0; i < children.Length(); i++) {
      Unused << children[i]->SendUnregisterSheet(uri, aSheetType);
    }
  }

  return NS_OK;
}

//static
nsStyleSheetService *
nsStyleSheetService::GetInstance()
{
  static bool first = true;
  if (first) {
    // make sure at first call that it's inited
    nsCOMPtr<nsIStyleSheetService> dummy =
      do_GetService(NS_STYLESHEETSERVICE_CONTRACTID);
    first = false;
  }

  return gInstance;
}

MOZ_DEFINE_MALLOC_SIZE_OF(StyleSheetServiceMallocSizeOf)

NS_IMETHODIMP
nsStyleSheetService::CollectReports(nsIHandleReportCallback* aHandleReport,
                                    nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/layout/style-sheet-service", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(StyleSheetServiceMallocSizeOf),
    "Memory used for style sheets held by the style sheet service.");

  return NS_OK;
}

size_t
nsStyleSheetService::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  for (auto& sheetArray : mSheets) {
    n += sheetArray.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (StyleSheet* sheet : sheetArray) {
      if (sheet) {
        n += sheet->SizeOfIncludingThis(aMallocSizeOf);
      }
    }
  }
  return n;
}

void
nsStyleSheetService::RegisterPresShell(nsIPresShell* aPresShell)
{
  MOZ_ASSERT(!mPresShells.Contains(aPresShell));
  mPresShells.AppendElement(aPresShell);
}

void
nsStyleSheetService::UnregisterPresShell(nsIPresShell* aPresShell)
{
  MOZ_ASSERT(mPresShells.Contains(aPresShell));
  mPresShells.RemoveElement(aPresShell);
}
