/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  if (
    !AppConstants.NIGHTLY_BUILD &&
    !AppConstants.MOZ_DEV_EDITION &&
    !AppConstants.DEBUG
  ) {
    ok(
      !("@mozilla.org/test/startuprecorder;1" in Cc),
      "the startup recorder component shouldn't exist in this non-nightly/non-devedition/" +
        "non-debug build."
    );
    return;
  }

  let startupRecorder =
    Cc["@mozilla.org/test/startuprecorder;1"].getService().wrappedJSObject;
  await startupRecorder.done;

  let extras = Cu.cloneInto(startupRecorder.data.extras, {});

  let phasesExpectations = {
    "before profile selection": false,
    "before opening first browser window": false,
    "before first paint": AppConstants.platform === "macosx",
    // Bug 1531854
    "before handling user events": true,
    "before becoming idle": true,
  };

  for (let phase in extras) {
    if (!(phase in phasesExpectations)) {
      ok(false, `Startup phase '${phase}' should be specified.`);
      continue;
    }

    is(
      extras[phase].hiddenWindowLoaded,
      phasesExpectations[phase],
      `Hidden window loaded at '${phase}': ${phasesExpectations[phase]}`
    );
  }
});
