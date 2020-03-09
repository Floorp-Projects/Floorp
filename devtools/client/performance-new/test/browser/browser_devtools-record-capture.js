/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info("Test that DevTools can capture profiles.");

  await setProfilerFrontendUrl(
    "http://example.com/browser/devtools/client/performance-new/test/browser/fake-frontend.html"
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
        "Capture recording"
      );
      info("Click the button to capture the recording.");
      button.click();
    }

    info(
      "If the DevTools successfully injects a profile into the page, then the " +
        "fake frontend will rename the title of the page."
    );

    await checkTabLoadedProfile({
      initialTitle: "Waiting on the profile",
      successTitle: "Profile received",
      errorTitle: "Error",
    });
  });

  const { revertRecordingPreferences } = ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm.js"
  );

  revertRecordingPreferences();
});
