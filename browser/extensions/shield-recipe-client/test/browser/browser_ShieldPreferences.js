"use strict";

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://shield-recipe-client/lib/AddonStudies.jsm", this);

const OPT_OUT_PREF = "app.shield.optoutstudies.enabled";

decorate_task(
  withMockPreferences,
  AddonStudies.withStudies([
    studyFactory({active: true}),
    studyFactory({active: true}),
  ]),
  async function testDisableStudiesWhenOptOutDisabled(mockPreferences, [study1, study2]) {
    mockPreferences.set(OPT_OUT_PREF, true);
    const observers = [
      studyEndObserved(study1.recipeId),
      studyEndObserved(study2.recipeId),
    ];
    Services.prefs.setBoolPref(OPT_OUT_PREF, false);
    await Promise.all(observers);

    const newStudy1 = await AddonStudies.get(study1.recipeId);
    const newStudy2 = await AddonStudies.get(study2.recipeId);
    ok(
      !newStudy1.active && !newStudy2.active,
      "Setting the opt-out pref to false stops all active opt-out studies."
    );
  }
);
