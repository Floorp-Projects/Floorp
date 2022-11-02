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
    ok(
      !Services.env.get("JS_TRACE_LOGGING"),
      "The JS_TRACE_LOGGING is not currently enabled."
    );
  }

  ok(
    false,
    "This test was migrated from the initial popup implementation to " +
      "about:profiling, however JS Tracer was disabled at the time. When " +
      "re-enabling JS Tracer, please audit that this text works as expected, " +
      "especially in the UI."
  );

  await withAboutProfiling(async document => {
    {
      info(
        "Test that there is offer to restart the browser when first loading up the popup."
      );
      const noRestartButton = maybeGetElementFromDocumentByText(
        document,
        "Restart"
      );
      ok(!noRestartButton, "There is no button to restart the browser.");
    }

    const jsTracerFeature = await getElementFromDocumentByText(
      document,
      "JSTracer"
    );

    {
      info("Toggle the jstracer feature on.");
      jsTracerFeature.click();

      const restartButton = await getElementFromDocumentByText(
        document,
        "Restart"
      );
      ok(
        restartButton,
        "There is now a button to offer to restart the browser"
      );
    }

    {
      info("Toggle the jstracer feature back off.");
      jsTracerFeature.click();

      const noRestartButton = maybeGetElementFromDocumentByText(
        document,
        "Restart"
      );
      ok(!noRestartButton, "The offer to restart the browser goes away.");
    }
  });
});
