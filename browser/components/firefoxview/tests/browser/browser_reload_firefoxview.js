/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
  Ensures that the Firefox View tab can be reloaded via:
  - Clicking the Refresh button in the toolbar
  - Using the various keyboard shortcuts
*/
add_task(async function test_reload_firefoxview() {
  await withFirefoxView({}, async browser => {
    let reloadButton = document.getElementById("reload-button");
    let tabLoaded = BrowserTestUtils.browserLoaded(browser);
    EventUtils.synthesizeMouseAtCenter(reloadButton, {}, browser.ownerGlobal);
    await tabLoaded;
    ok(true, "Firefox View loaded after clicking the Reload button");

    let keys = [
      ["R", { accelKey: true }],
      ["R", { accelKey: true, shift: true }],
      ["VK_F5", {}],
    ];

    if (AppConstants.platform != "macosx") {
      keys.push(["VK_F5", { accelKey: true }]);
    }

    for (let key of keys) {
      tabLoaded = BrowserTestUtils.browserLoaded(browser);
      EventUtils.synthesizeKey(key[0], key[1], browser.ownerGlobal);
      await tabLoaded;
      ok(true, `Firefox view loaded after using ${key}`);
    }
  });
});
