/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

var secureURL = "https://example.com/browser/browser/base/content/test/general/browser_star_hsts.sjs";
var unsecureURL = "http://example.com/browser/browser/base/content/test/general/browser_star_hsts.sjs";

add_task(async function test_star_redirect() {
  registerCleanupFunction(function() {
    // Ensure to remove example.com from the HSTS list.
    let sss = Cc["@mozilla.org/ssservice;1"]
                .getService(Ci.nsISiteSecurityService);
    sss.removeState(Ci.nsISiteSecurityService.HEADER_HSTS,
                    NetUtil.newURI("http://example.com/"), 0);
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
    gBrowser.removeCurrentTab();
  });

  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  // This will add the page to the HSTS cache.
  await promiseTabLoadEvent(tab, secureURL, secureURL);
  // This should transparently be redirected to the secure page.
  await promiseTabLoadEvent(tab, unsecureURL, secureURL);

  await promiseStarState(BookmarkingUI.STATUS_UNSTARRED);

  let bookmarkPanel = document.getElementById("editBookmarkPanel");
  let shownPromise = promisePopupShown(bookmarkPanel);
  BookmarkingUI.star.click();
  await shownPromise;

  is(BookmarkingUI.status, BookmarkingUI.STATUS_STARRED, "The star is starred");
});

/**
 * Waits for the star to reflect the expected state.
 */
function promiseStarState(aValue) {
  return new Promise(resolve => {
    let expectedStatus = aValue ? BookmarkingUI.STATUS_STARRED
                                : BookmarkingUI.STATUS_UNSTARRED;
    (function checkState() {
      if (BookmarkingUI.status == BookmarkingUI.STATUS_UPDATING ||
          BookmarkingUI.status != expectedStatus) {
        info("Waiting for star button change.");
        setTimeout(checkState, 1000);
      } else {
        resolve();
      }
    })();
  });
}

/**
 * Starts a load in an existing tab and waits for it to finish (via some event).
 *
 * @param aTab
 *        The tab to load into.
 * @param aUrl
 *        The url to load.
 * @param [optional] aFinalURL
 *        The url to wait for, same as aURL if not defined.
 * @return {Promise} resolved when the event is handled.
 */
function promiseTabLoadEvent(aTab, aURL, aFinalURL) {
  if (!aFinalURL)
    aFinalURL = aURL;

  info("Wait for load tab event");
  aTab.linkedBrowser.loadURI(aURL);
  return BrowserTestUtils.browserLoaded(aTab.linkedBrowser, false, aFinalURL);
}
