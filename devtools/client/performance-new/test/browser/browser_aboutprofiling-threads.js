/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function test() {
  info("Test that about:profiling can be loaded, and the threads changed.");

  await openAboutProfiling(async document => {
    const geckoMainLabel = await getElementFromDocumentByText(
      document,
      "GeckoMain"
    );
    const geckoMainInput = geckoMainLabel.querySelector("input");
    if (!geckoMainInput) {
      throw new Error("Unable to find the input from the GeckoMain label.");
    }

    ok(
      geckoMainInput.checked,
      "The GeckoMain thread starts checked by default."
    );

    ok(
      activeConfigurationHasThread("GeckoMain"),
      "The profiler was started with the GeckoMain thread"
    );

    info("Click the GeckoMain checkbox.");
    geckoMainInput.click();
    ok(!geckoMainInput.checked, "The GeckoMain thread UI is toggled off.");

    ok(
      !activeConfigurationHasThread("GeckoMain"),
      "The profiler was not started with the GeckoMain thread."
    );
  });

  const { revertRecordingPreferences } = ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm.js"
  );

  revertRecordingPreferences();
});
