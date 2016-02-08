"use strict";


/**
 * Verify that loading an invalid URI does not clobber a previously-loaded page's history
 * entry, but that the invalid URI gets its own history entry instead. We're checking this
 * using nsIWebNavigation's canGoBack, as well as actually going back and then checking
 * canGoForward.
 */
add_task(function* checkBackFromInvalidURI() {
  yield pushPrefs(["keyword.enabled", false]);
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots", true);
  gURLBar.value = "::2600";
  gURLBar.focus();

  let promiseErrorPageLoaded = new Promise(resolve => {
    tab.linkedBrowser.addEventListener("DOMContentLoaded", function onLoad() {
      tab.linkedBrowser.removeEventListener("DOMContentLoaded", onLoad, false, true);
      resolve();
    }, false, true);
  });
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield promiseErrorPageLoaded;

  ok(gBrowser.webNavigation.canGoBack, "Should be able to go back");
  if (gBrowser.webNavigation.canGoBack) {
    // Can't use DOMContentLoaded here because the page is bfcached. Can't use pageshow for
    // the error page because it doesn't seem to fire for those.
    let promiseOtherPageLoaded = BrowserTestUtils.waitForEvent(tab.linkedBrowser, "pageshow", false,
      // Be paranoid we *are* actually seeing this other page load, not some kind of race
      // for if/when we do start firing pageshow for the error page...
      function(e) { return gBrowser.currentURI.spec == "about:robots" }
    );
    gBrowser.goBack();
    yield promiseOtherPageLoaded;
    ok(gBrowser.webNavigation.canGoForward, "Should be able to go forward from previous page.");
  }
  yield BrowserTestUtils.removeTab(tab);
});
