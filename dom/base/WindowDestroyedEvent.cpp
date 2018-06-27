#include "WindowDestroyedEvent.h"

#include "nsJSUtils.h"
#include "jsapi.h"              // for JSAutoRequest
#include "js/Wrapper.h"
#include "nsIPrincipal.h"
#include "nsISupportsPrimitives.h"
#include "nsIAppStartup.h"
#include "nsToolkitCompsCID.h"
#include "nsCOMPtr.h"

namespace mozilla {

// Try to match compartments that are not web content by matching compartments
// with principals that are either the system principal or an expanded principal.
// This may not return true for all non-web-content compartments.
struct BrowserCompartmentMatcher : public js::CompartmentFilter {
  bool match(JS::Compartment* aC) const override
  {
    nsCOMPtr<nsIPrincipal> pc = nsJSPrincipals::get(JS_GetCompartmentPrincipals(aC));
    return nsContentUtils::IsSystemOrExpandedPrincipal(pc);
  }
};

WindowDestroyedEvent::WindowDestroyedEvent(nsGlobalWindowInner* aWindow,
                                           uint64_t aID, const char* aTopic)
  : mozilla::Runnable("WindowDestroyedEvent")
  , mID(aID)
  , mPhase(Phase::Destroying)
  , mTopic(aTopic)
  , mIsInnerWindow(true)
{
  mWindow = do_GetWeakReference(aWindow);
}

WindowDestroyedEvent::WindowDestroyedEvent(nsGlobalWindowOuter* aWindow,
                                           uint64_t aID, const char* aTopic)
  : mozilla::Runnable("WindowDestroyedEvent")
  , mID(aID)
  , mPhase(Phase::Destroying)
  , mTopic(aTopic)
  , mIsInnerWindow(false)
{
  mWindow = do_GetWeakReference(aWindow);
}

NS_IMETHODIMP
WindowDestroyedEvent::Run()
{
  AUTO_PROFILER_LABEL("WindowDestroyedEvent::Run", OTHER);

  nsCOMPtr<nsIObserverService> observerService =
    services::GetObserverService();
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
    case Phase::Destroying:
    {
      bool skipNukeCrossCompartment = false;
#ifndef DEBUG
      nsCOMPtr<nsIAppStartup> appStartup =
        do_GetService(NS_APPSTARTUP_CONTRACTID);

      if (appStartup) {
        appStartup->GetShuttingDown(&skipNukeCrossCompartment);
      }
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
        NS_IdleDispatchToCurrentThread(copy.forget(), 1000);
      }
    }
    break;

    case Phase::Nuking:
    {
      nsCOMPtr<nsISupports> window = do_QueryReferent(mWindow);
      if (window) {
        nsGlobalWindowInner* currentInner;
        if (mIsInnerWindow) {
          currentInner = nsGlobalWindowInner::FromSupports(window);
        } else {
          nsGlobalWindowOuter* outer = nsGlobalWindowOuter::FromSupports(window);
          currentInner = outer->GetCurrentInnerWindowInternal();
        }
        NS_ENSURE_TRUE(currentInner, NS_OK);

        AutoSafeJSContext cx;
        JS::Rooted<JSObject*> obj(cx, currentInner->FastGetGlobalJSObject());
        if (obj && !js::IsSystemRealm(js::GetNonCCWObjectRealm(obj))) {
          JS::Compartment* cpt = js::GetObjectCompartment(obj);
          nsCOMPtr<nsIPrincipal> pc = nsJSPrincipals::get(JS_GetCompartmentPrincipals(cpt));

          if (BasePrincipal::Cast(pc)->AddonPolicy()) {
            // We want to nuke all references to the add-on compartment.
            xpc::NukeAllWrappersForCompartment(cx, cpt,
                                               mIsInnerWindow ? js::DontNukeWindowReferences
                                                              : js::NukeWindowReferences);
          } else {
            // We only want to nuke wrappers for the chrome->content case
            js::NukeCrossCompartmentWrappers(cx, BrowserCompartmentMatcher(), cpt,
                                             mIsInnerWindow ? js::DontNukeWindowReferences
                                                            : js::NukeWindowReferences,
                                             js::NukeIncomingReferences);
          }
        }
      }
    }
    break;
  }

  return NS_OK;
}

} // namespace mozilla
