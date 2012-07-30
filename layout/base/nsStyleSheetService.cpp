/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of interface for managing user and user-agent style sheets */

#include "prlog.h"
#include "nsStyleSheetService.h"
#include "nsIStyleSheet.h"
#include "mozilla/css/Loader.h"
#include "nsCSSStyleSheet.h"
#include "nsIURI.h"
#include "nsContentCID.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"
#include "nsLayoutStatics.h"

nsStyleSheetService *nsStyleSheetService::gInstance = nullptr;

nsStyleSheetService::nsStyleSheetService()
{
  PR_STATIC_ASSERT(0 == AGENT_SHEET && 1 == USER_SHEET);
  NS_ASSERTION(!gInstance, "Someone is using CreateInstance instead of GetService");
  gInstance = this;
  nsLayoutStatics::AddRef();
}

nsStyleSheetService::~nsStyleSheetService()
{
  gInstance = nullptr;
  nsLayoutStatics::Release();
}

NS_IMPL_ISUPPORTS1(nsStyleSheetService, nsIStyleSheetService)

void
nsStyleSheetService::RegisterFromEnumerator(nsICategoryManager  *aManager,
                                            const char          *aCategory,
                                            nsISimpleEnumerator *aEnumerator,
                                            PRUint32             aSheetType)
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

    nsCAutoString name;
    icStr->GetData(name);

    nsXPIDLCString spec;
    aManager->GetCategoryEntry(aCategory, name.get(), getter_Copies(spec));

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), spec);
    if (uri)
      LoadAndRegisterSheetInternal(uri, aSheetType);
  }
}

PRInt32
nsStyleSheetService::FindSheetByURI(const nsCOMArray<nsIStyleSheet> &sheets,
                                    nsIURI *sheetURI)
{
  for (PRInt32 i = sheets.Count() - 1; i >= 0; i-- ) {
    bool bEqual;
    nsIURI* uri = sheets[i]->GetSheetURI();
    if (uri
        && NS_SUCCEEDED(uri->Equals(sheetURI, &bEqual))
        && bEqual) {
      return i;
    }
  }

  return -1;
}

nsresult
nsStyleSheetService::Init()
{
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

  return NS_OK;
}

NS_IMETHODIMP
nsStyleSheetService::LoadAndRegisterSheet(nsIURI *aSheetURI,
                                          PRUint32 aSheetType)
{
  nsresult rv = LoadAndRegisterSheetInternal(aSheetURI, aSheetType);
  if (NS_SUCCEEDED(rv)) {
    const char* message = (aSheetType == AGENT_SHEET) ?
      "agent-sheet-added" : "user-sheet-added";
    nsCOMPtr<nsIObserverService> serv =
      mozilla::services::GetObserverService();
    if (serv) {
      // We're guaranteed that the new sheet is the last sheet in
      // mSheets[aSheetType]
      const nsCOMArray<nsIStyleSheet> & sheets = mSheets[aSheetType];
      serv->NotifyObservers(sheets[sheets.Count() - 1], message, nullptr);
    }
  }
  return rv;
}

nsresult
nsStyleSheetService::LoadAndRegisterSheetInternal(nsIURI *aSheetURI,
                                                  PRUint32 aSheetType)
{
  NS_ENSURE_ARG(aSheetType == AGENT_SHEET || aSheetType == USER_SHEET);
  NS_ENSURE_ARG_POINTER(aSheetURI);

  nsRefPtr<mozilla::css::Loader> loader = new mozilla::css::Loader();

  nsRefPtr<nsCSSStyleSheet> sheet;
  // Allow UA sheets, but not user sheets, to use unsafe rules
  nsresult rv = loader->LoadSheetSync(aSheetURI, aSheetType == AGENT_SHEET,
                                      true, getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mSheets[aSheetType].AppendObject(sheet)) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

NS_IMETHODIMP
nsStyleSheetService::SheetRegistered(nsIURI *sheetURI,
                                     PRUint32 aSheetType, bool *_retval)
{
  NS_ENSURE_ARG(aSheetType == AGENT_SHEET || aSheetType == USER_SHEET);
  NS_ENSURE_ARG_POINTER(sheetURI);
  NS_PRECONDITION(_retval, "Null out param");

  *_retval = (FindSheetByURI(mSheets[aSheetType], sheetURI) >= 0);

  return NS_OK;
}

NS_IMETHODIMP
nsStyleSheetService::UnregisterSheet(nsIURI *sheetURI, PRUint32 aSheetType)
{
  NS_ENSURE_ARG(aSheetType == AGENT_SHEET || aSheetType == USER_SHEET);
  NS_ENSURE_ARG_POINTER(sheetURI);

  PRInt32 foundIndex = FindSheetByURI(mSheets[aSheetType], sheetURI);
  NS_ENSURE_TRUE(foundIndex >= 0, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsIStyleSheet> sheet = mSheets[aSheetType][foundIndex];
  mSheets[aSheetType].RemoveObjectAt(foundIndex);
  
  const char* message = (aSheetType == AGENT_SHEET) ?
      "agent-sheet-removed" : "user-sheet-removed";
  nsCOMPtr<nsIObserverService> serv =
    mozilla::services::GetObserverService();
  if (serv)
    serv->NotifyObservers(sheet, message, nullptr);

  return NS_OK;
}
