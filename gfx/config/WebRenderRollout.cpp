/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderRollout.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupportsImpl.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace gfx {

static const char* const WR_ROLLOUT_PREF = "gfx.webrender.all.qualified";
static const bool WR_ROLLOUT_PREF_DEFAULTVALUE = true;
static const char* const WR_ROLLOUT_DEFAULT_PREF =
    "gfx.webrender.all.qualified.default";
static const bool WR_ROLLOUT_DEFAULT_PREF_DEFAULTVALUE = false;
static const char* const WR_ROLLOUT_PREF_OVERRIDE =
    "gfx.webrender.all.qualified.gfxPref-default-override";
static const char* const WR_ROLLOUT_HW_QUALIFIED_OVERRIDE =
    "gfx.webrender.all.qualified.hardware-override";
static const char* const PROFILE_BEFORE_CHANGE_TOPIC = "profile-before-change";

// If the "gfx.webrender.all.qualified" pref is true we want to enable
// WebRender for qualified hardware. This pref may be set by the Normandy
// Preference Rollout feature. The Normandy pref rollout code sets default
// values on rolled out prefs on every startup. Default pref values are not
// persisted; they only exist in memory for that session. Gfx starts up
// before Normandy does. So it's too early to observe the WR qualified pref
// changed by Normandy rollout on gfx startup. So we add a shutdown observer to
// save the default value on shutdown, and read the saved value on startup
// instead.
class WrRolloutPrefShutdownSaver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports*, const char* aTopic,
                     const char16_t*) override {
    if (strcmp(PROFILE_BEFORE_CHANGE_TOPIC, aTopic) != 0) {
      // Not the observer we're looking for, move along.
      return NS_OK;
    }

    SaveRolloutPref();

    // Shouldn't receive another notification, remove the observer.
    RefPtr<WrRolloutPrefShutdownSaver> kungFuDeathGrip(this);
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return NS_ERROR_FAILURE;
    }
    observerService->RemoveObserver(this, PROFILE_BEFORE_CHANGE_TOPIC);
    return NS_OK;
  }

  static void AddShutdownObserver() {
    MOZ_ASSERT(XRE_IsParentProcess());
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return;
    }
    RefPtr<WrRolloutPrefShutdownSaver> wrRolloutSaver =
        new WrRolloutPrefShutdownSaver();
    observerService->AddObserver(wrRolloutSaver, PROFILE_BEFORE_CHANGE_TOPIC,
                                 false);
  }

 private:
  virtual ~WrRolloutPrefShutdownSaver() = default;

  void SaveRolloutPref() {
    if (Preferences::HasUserValue(WR_ROLLOUT_PREF) ||
        Preferences::GetType(WR_ROLLOUT_PREF) == nsIPrefBranch::PREF_INVALID) {
      // Don't need to create a backup of default value, because either:
      // 1. the user or the WR SHIELD study has set a user pref value, or
      // 2. we've not had a default pref set by Normandy that needs to be saved
      //    for reading before Normandy has started up.
      return;
    }

    bool defaultValue =
        Preferences::GetBool(WR_ROLLOUT_PREF, false, PrefValueKind::Default);
    Preferences::SetBool(WR_ROLLOUT_DEFAULT_PREF, defaultValue);
  }
};

NS_IMPL_ISUPPORTS(WrRolloutPrefShutdownSaver, nsIObserver)

/* static */ void WebRenderRollout::Init() {
  WrRolloutPrefShutdownSaver::AddShutdownObserver();
}

/* static */ Maybe<bool> WebRenderRollout::CalculateQualifiedOverride() {
  // This pref only ever gets set in test_pref_rollout_workaround, and in
  // that case we want to ignore the MOZ_WEBRENDER=0 that will be set by
  // the test harness so as to actually make the test work.
  if (!Preferences::HasUserValue(WR_ROLLOUT_HW_QUALIFIED_OVERRIDE)) {
    return Nothing();
  }
  return Some(Preferences::GetBool(WR_ROLLOUT_HW_QUALIFIED_OVERRIDE, false));
}

// If the "gfx.webrender.all.qualified" pref is true we want to enable
// WebRender for qualifying hardware. The Normandy pref rollout code sets
// default values on rolled out prefs on every startup, but Gfx starts up
// before Normandy does. So it's too early to observe the WR qualified pref
// default value changed by Normandy rollout here yet. So we have a shutdown
// observer to save the default value on shutdown, and read the saved default
// value here instead, and emulate the behavior of the pref system, with
// respect to default/user values of the rollout pref.
/* static */ bool WebRenderRollout::CalculateQualified() {
  auto clearPrefOnExit = MakeScopeExit([]() {
    // Clear the mirror of the default value of the rollout pref on scope exit,
    // if we have one. This ensures the user doesn't mess with the pref.
    // If we need it again, we'll re-create it on shutdown.
    Preferences::ClearUser(WR_ROLLOUT_DEFAULT_PREF);
  });

  if (!Preferences::HasUserValue(WR_ROLLOUT_PREF) &&
      Preferences::HasUserValue(WR_ROLLOUT_DEFAULT_PREF)) {
    // The user has not set a user pref, and we have a default value set by the
    // shutdown observer. Let's use this as it should be the value Normandy set
    // before startup. WR_ROLLOUT_DEFAULT_PREF should only be set on shutdown by
    // the shutdown observer.
    // Normandy runs *during* startup, but *after* this code here runs (hence
    // the need for the workaround).
    // To have a value stored in the WR_ROLLOUT_DEFAULT_PREF pref here, during
    // the previous run Normandy must have set a default value on the in-memory
    // pref, and on shutdown we stored the default value in this
    // WR_ROLLOUT_DEFAULT_PREF user pref. Then once the user restarts, we
    // observe this pref. Normandy is the only way a default (not user) value
    // can be set for this pref.
    return Preferences::GetBool(WR_ROLLOUT_DEFAULT_PREF,
                                WR_ROLLOUT_DEFAULT_PREF_DEFAULTVALUE);
  }

  // We don't have a user value for the rollout pref, and we don't have the
  // value of the rollout pref at last shutdown stored. So we should fallback
  // to using the default. *But* if we're running
  // under the Marionette pref rollout work-around test, we may want to override
  // the default value expressed here, so we can test the "default disabled;
  // rollout pref enabled" case.
  // Note that those preferences can't be defined in all.js nor
  // StaticPrefsList.h as they would create the pref, leading SaveRolloutPref()
  // above to abort early as the pref would have a valid type.
  //  We also don't want those prefs to appear in about:config.
  if (Preferences::HasUserValue(WR_ROLLOUT_PREF_OVERRIDE)) {
    return Preferences::GetBool(WR_ROLLOUT_PREF_OVERRIDE);
  }
  return Preferences::GetBool(WR_ROLLOUT_PREF, WR_ROLLOUT_PREF_DEFAULTVALUE);
}

}  // namespace gfx
}  // namespace mozilla
