/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function bookmarks_toolbar_shown_on_newtab() {
  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "slow_loading_page.sjs";
  for (let featureEnabled of [true, false]) {
    for (let newTabEnabled of [true, false]) {
      info(
        `Testing with the feature ${
          featureEnabled ? "enabled" : "disabled"
        } and newtab ${newTabEnabled ? "enabled" : "disabled"}`
      );
      await SpecialPowers.pushPrefEnv({
        set: [
          ["browser.toolbars.bookmarks.2h2020", featureEnabled],
          ["browser.newtabpage.enabled", newTabEnabled],
        ],
      });

      let newWindowOpened = BrowserTestUtils.domWindowOpened();
      let triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
      openUILinkIn(url, "window", { triggeringPrincipal });

      let newWin = await newWindowOpened;

      let exitConditions = {
        visible: true,
      };
      let slowSiteLoaded = BrowserTestUtils.firstBrowserLoaded(newWin, false);
      slowSiteLoaded.then(result => {
        exitConditions.earlyExit = result;
      });

      let result = await waitForBookmarksToolbarVisibilityWithExitConditions({
        win: newWin,
        exitConditions,
        message: "Toolbar should not become visible when loading slow site",
      });

      // The visibility promise will resolve to a Boolean whereas the browser
      // load promise will resolve to an Event object.
      ok(
        typeof result != "boolean",
        "The bookmarks toolbar should not become visible before the site is loaded"
      );
      ok(!isBookmarksToolbarVisible(newWin), "Toolbar hidden on slow site");

      await BrowserTestUtils.closeWindow(newWin);
    }
  }
});
