/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_openPreferences_aboutSettings() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:settings",
    },
    async () => {
      is(
        gBrowser.currentURI.spec,
        "about:settings",
        "about:settings should open normally"
      );

      //  using `openPreferencesViaOpenPreferencesAPI` would introduce an extra about:blank tab we need to take care of
      await openPreferences("paneGeneral");

      is(
        gBrowser.currentURI.spec,
        "about:settings#general",
        "openPreferences should keep about:settings"
      );
    }
  );
});
