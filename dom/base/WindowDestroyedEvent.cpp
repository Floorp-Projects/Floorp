/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowDestroyedEvent.h"

#include "nsJSUtils.h"
#include "jsapi.h"
#include "js/Wrapper.h"
#include "nsIPrincipal.h"
#include "nsISupportsPrimitives.h"
#include "nsIAppStartup.h"
#include "nsJSPrincipals.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsGlobalWindowOuter.h"
#include "xpcpublic.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Components.h"
#include "mozilla/ProfilerLabels.h"
#include "nsFocusManager.h"

namespace mozilla {

struct BrowserCompartmentMatcher : public js::CompartmentFilter {
  bool match(JS::Compartment* aC) const override {
    return !xpc::MightBeWebContentCompartment(aC);
  }
};

WindowDestroyedEvent::WindowDestroyedEvent(nsGlobalWindowInner* aWindow,
                                           uint64_t aID, const char* aTopic)
    : mozilla::Runnable("WindowDestroyedEvent"),
      mID(aID),
      mPhase(Phase::Destroying),
      mTopic(aTopic),
      mIsInnerWindow(true) {
  mWindow = do_GetWeakReference(aWindow);
}

WindowDestroyedEvent::WindowDestroyedEvent(nsGlobalWindowOuter* aWindow,
                                           uint64_t aID, const char* aTopic)
    : mozilla::Runnable("WindowDestroyedEvent"),
      mID(aID),
      mPhase(Phase::Destroying),
      mTopic(aTopic),
      mIsInnerWindow(false) {
  mWindow = do_GetWeakReference(aWindow);
}

NS_IMETHODIMP
WindowDestroyedEvent::Run() {
  AUTO_PROFILER_LABEL("WindowDestroyedEvent::Run", OTHER);

  nsCOMPtr<nsPIDOMWindowOuter> nukedOuter;

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (!observerService) {
    return NS_OK;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper =
      do_CreateInstance(NS_SUPPORTS_PRUINT64_CONTRACTID);
  if (wrapper) {
    wrapper->SetData(mID);
    observerService->NotifyObservers(wrapper, mTopic.get(), nullptr);
  }

  switch (mPhase) {
    case Phase::Destroying: {
      bool skipNukeCrossCompartment = false;
#ifndef DEBUG
      skipNukeCrossCompartment =
          AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed);
#endif

      if (!skipNukeCrossCompartment) {
        // The compartment nuking phase might be too expensive, so do that
        // part off of idle dispatch.

        // For the compartment nuking phase, we dispatch either an
        // inner-window-nuked or an outer-window-nuked notification.
        // This will allow tests to wait for compartment nuking to happen.
        if (mTopic.EqualsLiteral("inner-window-destroyed")) {
          mTopic.AssignLiteral("inner-window-nuked");
        } else if (mTopic.EqualsLiteral("outer-window-destroyed")) {
          mTopic.AssignLiteral("outer-window-nuked");
        }
        mPhase = Phase::Nuking;

        nsCOMPtr<nsIRunnable> copy(this);
        NS_DispatchToCurrentThreadQueue(copy.forget(), 1000,
                                        EventQueuePriority::Idle);
      }
    } break;

    case Phase::Nuking: {
      nsCOMPtr<nsISupports> window = do_QueryReferent(mWindow);
      if (window) {
        nsGlobalWindowInner* currentInner;
        if (mIsInnerWindow) {
          currentInner = nsGlobalWindowInner::FromSupports(window);
        } else {
          nsGlobalWindowOuter* outer =
              nsGlobalWindowOuter::FromSupports(window);
          currentInner =
              nsGlobalWindowInner::Cast(outer->GetCurrentInnerWindow());
          nukedOuter = outer;
        }
        NS_ENSURE_TRUE(currentInner, NS_OK);

        dom::AutoJSAPI jsapi;
        jsapi.Init();
        JSContext* cx = jsapi.cx();
        JS::Rooted<JSObject*> obj(cx, currentInner->GetGlobalJSObject());
        if (obj && !js::IsSystemRealm(js::GetNonCCWObjectRealm(obj))) {
          JS::Realm* realm = js::GetNonCCWObjectRealm(obj);

          xpc::NukeJSStackFrames(realm);

          nsCOMPtr<nsIPrincipal> pc =
              nsJSPrincipals::get(JS::GetRealmPrincipals(realm));

          if (BasePrincipal::Cast(pc)->AddonPolicy()) {
            // We want to nuke all references to the add-on realm.
            xpc::NukeAllWrappersForRealm(cx, realm,
                                         mIsInnerWindow
                                             ? js::DontNukeWindowReferences
                                             : js::NukeWindowReferences);
          } else {
            // We only want to nuke wrappers for the chrome->content case
            js::NukeCrossCompartmentWrappers(
                cx, BrowserCompartmentMatcher(), realm,
                mIsInnerWindow ? js::DontNukeWindowReferences
                               : js::NukeWindowReferences,
                js::NukeIncomingReferences);
          }
        }
      }
    } break;
  }

  if (nukedOuter) {
    nsFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      fm->WasNuked(nukedOuter);
    }
  }

  return NS_OK;
}

}  // namespace mozilla
