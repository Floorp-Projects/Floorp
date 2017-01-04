/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var secureURL = "https://example.com/browser/browser/base/content/test/general/browser_star_hsts.sjs";
var unsecureURL = "http://example.com/browser/browser/base/content/test/general/browser_star_hsts.sjs";

add_task(function* test_star_redirect() {
  registerCleanupFunction(function() {
    // Ensure to remove example.com from the HSTS list.
    let sss = Cc["@mozilla.org/ssservice;1"]
                .getService(Ci.nsISiteSecurityService);
    sss.removeState(Ci.nsISiteSecurityService.HEADER_HSTS,
                    NetUtil.newURI("http://example.com/"), 0);
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
    gBrowser.removeCurrentTab();
  });

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  // This will add the page to the HSTS cache.
  yield promiseTabLoadEvent(tab, secureURL, secureURL);
  // This should transparently be redirected to the secure page.
  yield promiseTabLoadEvent(tab, unsecureURL, secureURL);

  yield promiseStarState(BookmarkingUI.STATUS_UNSTARRED);

  let promiseBookmark = promiseOnBookmarkItemAdded(gBrowser.currentURI);
  BookmarkingUI.star.click();
  // This resolves on the next tick, so the star should have already been
  // updated at that point.
  yield promiseBookmark;

  is(BookmarkingUI.status, BookmarkingUI.STATUS_STARRED, "The star is starred");
});

/**
 * Waits for the star to reflect the expected state.
 */
function promiseStarState(aValue) {
  let deferred = Promise.defer();
  let expectedStatus = aValue ? BookmarkingUI.STATUS_STARRED
                              : BookmarkingUI.STATUS_UNSTARRED;
  (function checkState() {
    if (BookmarkingUI.status == BookmarkingUI.STATUS_UPDATING ||
        BookmarkingUI.status != expectedStatus) {
      info("Waiting for star button change.");
      setTimeout(checkState, 1000);
    } else {
      deferred.resolve();
    }
  })();
  return deferred.promise;
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
  let deferred = Promise.defer();
  info("Wait for load tab event");
  aTab.linkedBrowser.addEventListener("load", function load(event) {
    if (event.originalTarget != aTab.linkedBrowser.contentDocument ||
        event.target.location.href == "about:blank" ||
        event.target.location.href != aFinalURL) {
      info("skipping spurious load event");
      return;
    }
    aTab.linkedBrowser.removeEventListener("load", load, true);
    info("Tab load event received");
    deferred.resolve();
  }, true, true);
  aTab.linkedBrowser.loadURI(aURL);
  return deferred.promise;
}
