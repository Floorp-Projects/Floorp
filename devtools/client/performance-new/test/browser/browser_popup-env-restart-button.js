/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info(
    "Test that the popup offers to restart the browser to set an enviroment flag."
  );

  if (!Services.profiler.GetFeatures().includes("jstracer")) {
    ok(
      true,
      "JS tracer is not supported on this platform, or is currently disabled. Skip the rest of the test."
    );
    return;
  }

  {
    info("Ensure that JS Tracer is not currently enabled.");
    const {
      getEnvironmentVariable,
    } = require("devtools/client/performance-new/browser");
    ok(
      !getEnvironmentVariable("JS_TRACE_LOGGING"),
      "The JS_TRACE_LOGGING is not currently enabled."
    );
  }

  await makeSureProfilerPopupIsEnabled();
  toggleOpenProfilerPopup();

  {
    info("Open up the features section.");
    const features = await getElementFromPopupByText("Features:");
    features.click();
  }

  {
    info(
      "Test that there is offer to restart the browser when first loading up the popup."
    );
    const noRestartButton = maybeGetElementFromPopupByText("Restart");
    ok(!noRestartButton, "There is no button to restart the browser.");
  }

  const jsTracerFeature = await getElementFromPopupByText("JSTracer");

  {
    info("Toggle the jstracer feature on.");
    jsTracerFeature.click();

    const restartButton = await getElementFromPopupByText("Restart");
    ok(restartButton, "There is now a button to offer to restart the browser");
  }

  {
    info("Toggle the jstracer feature back off.");
    jsTracerFeature.click();

    const noRestartButton = maybeGetElementFromPopupByText("Restart");
    ok(!noRestartButton, "The offer to restart the browser goes away.");
  }

  await closePopup();
});
