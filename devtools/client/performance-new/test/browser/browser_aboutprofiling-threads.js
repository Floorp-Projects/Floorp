/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function test() {
  info("Test that about:profiling can be loaded, and the threads changed.");

  await withAboutProfiling(async document => {
    const geckoMainInput = await getNearestInputFromText(document, "GeckoMain");

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
});
