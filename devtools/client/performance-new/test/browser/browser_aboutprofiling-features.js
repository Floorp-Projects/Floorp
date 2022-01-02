/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info("Test that about:profiling can be loaded, and the features changed.");

  ok(
    Services.profiler.GetFeatures().includes("js"),
    "This test assumes that the JavaScript feature is available on every platform."
  );

  await withAboutProfiling(async document => {
    const jsInput = await getNearestInputFromText(document, "JavaScript");

    ok(
      activeConfigurationHasFeature("js"),
      "By default, the JS feature is always enabled."
    );
    ok(jsInput.checked, "The JavaScript input is checked when enabled.");

    jsInput.click();

    ok(
      !activeConfigurationHasFeature("js"),
      "The JS feature can be toggled off."
    );
    ok(!jsInput.checked, "The JS feature's input element is also toggled off.");
  });
});
