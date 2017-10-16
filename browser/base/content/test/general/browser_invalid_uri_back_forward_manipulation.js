"use strict";


/**
 * Verify that loading an invalid URI does not clobber a previously-loaded page's history
 * entry, but that the invalid URI gets its own history entry instead. We're checking this
 * using nsIWebNavigation's canGoBack, as well as actually going back and then checking
 * canGoForward.
 */
add_task(async function checkBackFromInvalidURI() {
  await pushPrefs(["keyword.enabled", false]);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots", true);
  info("Loaded about:robots");

  gURLBar.value = "::2600";

  let promiseErrorPageLoaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  gURLBar.handleCommand();
  await promiseErrorPageLoaded;

  ok(gBrowser.webNavigation.canGoBack, "Should be able to go back");
  if (gBrowser.webNavigation.canGoBack) {
    // Can't use DOMContentLoaded here because the page is bfcached. Can't use pageshow for
    // the error page because it doesn't seem to fire for those.
    let promiseOtherPageLoaded = BrowserTestUtils.waitForEvent(tab.linkedBrowser, "pageshow", false,
      // Be paranoid we *are* actually seeing this other page load, not some kind of race
      // for if/when we do start firing pageshow for the error page...
      function(e) { return gBrowser.currentURI.spec == "about:robots"; }
    );
    gBrowser.goBack();
    await promiseOtherPageLoaded;
    ok(gBrowser.webNavigation.canGoForward, "Should be able to go forward from previous page.");
  }
  await BrowserTestUtils.removeTab(tab);
});
