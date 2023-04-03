/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if somehow about:newtab loads before about:home does, that we
 * don't use the cache. This is because about:newtab doesn't use the cache,
 * and so it'll inevitably be newer than what's in the about:home cache,
 * which will put the about:home cache out of date the next time about:home
 * eventually loads.
 */
add_task(async function test_no_cache_on_SessionStartup_restore() {
  await withFullyLoadedAboutHome(async browser => {
    await simulateRestart(browser, { skipAboutHomeLoad: true });

    // We remove the preloaded browser to ensure that loading the next
    // about:newtab occurs now, and not at preloading time.
    NewTabPagePreloading.removePreloadedBrowser(window);
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:newtab"
    );

    let newWin = await BrowserTestUtils.openNewBrowserWindow();

    // The cache is disqualified because about:newtab was loaded first.
    // So now it's too late to use the cache.
    await ensureDynamicAboutHome(
      newWin.gBrowser.selectedBrowser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.LATE
    );

    await BrowserTestUtils.closeWindow(newWin);
    await BrowserTestUtils.removeTab(tab);
  });
});
