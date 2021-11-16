/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info("Test that DevTools can discard profiles.");

  await setProfilerFrontendUrl(
    "http://example.com",
    "/browser/devtools/client/performance-new/test/browser/fake-frontend.html"
  );

  await withDevToolsPanel(async document => {
    {
      const button = await getActiveButtonFromText(document, "Start recording");
      info("Click the button to start recording");
      button.click();
    }

    {
      const button = await getActiveButtonFromText(
        document,
        "Cancel recording"
      );
      info("Click the button to discard to profile.");
      button.click();
    }

    {
      const button = await getActiveButtonFromText(document, "Start recording");
      ok(Boolean(button), "The start recording button is available again.");
    }
  });
});
