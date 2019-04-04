/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/UIDirectionManager.h"
#include "mozilla/Preferences.h"
#include "nsIWindowMediator.h"
#include "nsDocShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/SimpleEnumerator.h"

namespace mozilla {
namespace dom {

/* static */
void OnPrefChange(const char* aPrefName, void*) {
  // Iterate over all of the windows and notify them of the direction change.
  nsCOMPtr<nsIWindowMediator> windowMediator =
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
  NS_ENSURE_TRUE_VOID(windowMediator);

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  windowMediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
  NS_ENSURE_TRUE_VOID(windowEnumerator);

  for (auto& elements : SimpleEnumerator<nsISupports>(windowEnumerator)) {
    nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(elements);
    if (window->Closed()) {
      continue;
    }

    nsCOMPtr<nsIDocShell> rootDocShell = window->GetDocShell();
    nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
    rootDocShell->GetDocShellEnumerator(nsIDocShell::typeAll,
                                        nsIDocShell::ENUMERATE_FORWARDS,
                                        getter_AddRefs(docShellEnumerator));
    NS_ENSURE_TRUE_VOID(docShellEnumerator);
    for (auto& docShell : SimpleEnumerator<nsIDocShell>(docShellEnumerator)) {
      if (nsCOMPtr<nsPIDOMWindowOuter> win = do_GetInterface(docShell)) {
        if (dom::Document* doc = win->GetExtantDoc()) {
          doc->ResetDocumentDirection();
        }
      }
    }
  }
}

/* static */
void UIDirectionManager::Initialize() {
  DebugOnly<nsresult> rv =
      Preferences::RegisterCallback(OnPrefChange, "intl.uidirection");
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed to observe \"intl.uidirection\"");
}

/* static */
void UIDirectionManager::Shutdown() {
  Preferences::UnregisterCallback(OnPrefChange, "intl.uidirection");
}

}  // namespace dom
}  // namespace mozilla
