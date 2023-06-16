/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/UIDirectionManager.h"
#include "mozilla/Preferences.h"
#include "nsIObserverService.h"
#include "nsIWindowMediator.h"
#include "nsDocShell.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/Services.h"
#include "mozilla/SimpleEnumerator.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(UIDirectionManager, nsIObserver)

/* static */
NS_IMETHODIMP
UIDirectionManager::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData) {
  NS_ENSURE_FALSE(strcmp(aTopic, "intl:app-locales-changed"), NS_ERROR_FAILURE);

  // Iterate over all of the windows and notify them of the direction change.
  nsCOMPtr<nsIWindowMediator> windowMediator =
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
  NS_ENSURE_TRUE(windowMediator, NS_ERROR_FAILURE);

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  windowMediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
  NS_ENSURE_TRUE(windowEnumerator, NS_ERROR_FAILURE);

  for (auto& elements : SimpleEnumerator<nsISupports>(windowEnumerator)) {
    nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(elements);
    if (window->Closed()) {
      continue;
    }

    RefPtr<BrowsingContext> context = window->GetBrowsingContext();
    MOZ_DIAGNOSTIC_ASSERT(context);

    if (context->IsDiscarded()) {
      continue;
    }

    context->PreOrderWalk([](BrowsingContext* aContext) {
      if (dom::Document* doc = aContext->GetDocument()) {
        doc->ResetDocumentDirection();
      }
    });
  }
  return NS_OK;
}

/* static */
void UIDirectionManager::Initialize() {
  MOZ_ASSERT(!gUIDirectionManager);
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<UIDirectionManager> observer = new UIDirectionManager();

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }
  obs->AddObserver(observer, "intl:app-locales-changed", false);

  gUIDirectionManager = observer;
}

/* static */
void UIDirectionManager::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!gUIDirectionManager) {
    return;
  }
  RefPtr<UIDirectionManager> observer = gUIDirectionManager;
  gUIDirectionManager = nullptr;

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return;
  }

  obs->RemoveObserver(observer, "intl:app-locales-changed");
}

mozilla::StaticRefPtr<UIDirectionManager>
    UIDirectionManager::gUIDirectionManager;

}  // namespace mozilla::dom
