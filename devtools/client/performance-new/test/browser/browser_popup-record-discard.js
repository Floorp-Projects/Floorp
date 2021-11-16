/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info("Test that the profiler popup recording can be discarded.");
  await setProfilerFrontendUrl(
    "http://example.com",
    "/browser/devtools/client/performance-new/test/browser/fake-frontend.html"
  );
  await makeSureProfilerPopupIsEnabled();
  await withPopupOpen(window, async () => {
    {
      const button = await getElementByLabel(document, "Start Recording");
      info("Click the button to start recording.");
      button.click();
    }

    {
      const button = await getElementByLabel(document, "Discard");
      info("Click the button to discard the recording.");
      button.click();
    }

    {
      const button = await getElementByLabel(document, "Start Recording");
      ok(
        Boolean(button),
        "The popup reverted back to be able to start recording again"
      );
    }
  });
});
