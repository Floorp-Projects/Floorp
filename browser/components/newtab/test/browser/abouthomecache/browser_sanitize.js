/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that when sanitizing places history, session store or downloads, that
 * the about:home cache gets blown away.
 */

add_task(async function test_sanitize() {
  let testFlags = [
    ["downloads", Ci.nsIClearDataService.CLEAR_DOWNLOADS],
    ["places history", Ci.nsIClearDataService.CLEAR_HISTORY],
    ["session history", Ci.nsIClearDataService.CLEAR_SESSION_HISTORY],
  ];

  await BrowserTestUtils.withNewTab("about:home", async browser => {
    for (let [type, flag] of testFlags) {
      await simulateRestart(browser);
      await ensureCachedAboutHome(browser);

      info(
        "Testing that the about:home startup cache is cleared when " +
          `clearing ${type}`
      );

      await new Promise((resolve, reject) => {
        Services.clearData.deleteData(flag, {
          onDataDeleted(resultFlags) {
            if (!resultFlags) {
              resolve();
            } else {
              reject(new Error(`Failed with flags: ${resultFlags}`));
            }
          },
        });
      });

      await simulateRestart(browser, { withAutoShutdownWrite: false });
      await ensureDynamicAboutHome(
        browser,
        AboutHomeStartupCache.CACHE_RESULT_SCALARS.DOES_NOT_EXIST
      );
    }
  });
});
