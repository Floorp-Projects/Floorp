/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/Telemetry.h"
#include "nsIDocShell.h"
#include "nsIEffectiveTLDService.h"
#include "nsIURI.h"

namespace mozilla {
namespace dom {

/* static */ void
DocGroup::GetKey(nsIPrincipal* aPrincipal, nsACString& aKey)
{
  aKey.Truncate();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  // GetBaseDomain works fine if |uri| is null, but it outputs a warning
  // which ends up cluttering the logs.
  if (NS_SUCCEEDED(rv) && uri) {
    nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
    if (tldService) {
      rv = tldService->GetBaseDomain(uri, 0, aKey);
      if (NS_FAILED(rv)) {
        aKey.Truncate();
      }
    }
  }
}

void
DocGroup::RemoveDocument(nsIDocument* aDocument)
{
  MOZ_ASSERT(mDocuments.Contains(aDocument));
  mDocuments.RemoveElement(aDocument);
}

DocGroup::DocGroup(TabGroup* aTabGroup, const nsACString& aKey)
  : mKey(aKey), mTabGroup(aTabGroup)
{
  // This method does not add itself to mTabGroup->mDocGroups as the caller does it for us.
}

DocGroup::~DocGroup()
{
  MOZ_ASSERT(mDocuments.IsEmpty());
  mTabGroup->mDocGroups.RemoveEntry(mKey);
}

NS_IMPL_ISUPPORTS(DocGroup, nsISupports)

nsresult
DocGroup::Dispatch(const char* aName,
                   TaskCategory aCategory,
                   already_AddRefed<nsIRunnable>&& aRunnable)
{
  return mTabGroup->Dispatch(aName, aCategory, Move(aRunnable));
}

nsIEventTarget*
DocGroup::EventTargetFor(TaskCategory aCategory) const
{
  return mTabGroup->EventTargetFor(aCategory);
}

}
}
