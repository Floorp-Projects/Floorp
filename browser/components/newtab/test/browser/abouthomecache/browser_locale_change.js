/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the about:home startup cache is cleared if the app
 * locale changes.
 */
add_task(async function test_locale_change() {
  await BrowserTestUtils.withNewTab("about:home", async browser => {
    await simulateRestart(browser);
    await ensureCachedAboutHome(browser);

    Services.obs.notifyObservers(null, "intl:app-locales-changed");

    await simulateRestart(browser, false);
    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.DOES_NOT_EXIST
    );
  });
});
