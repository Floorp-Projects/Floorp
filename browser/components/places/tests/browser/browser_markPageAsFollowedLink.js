/**
 * Tests that visits across frames are correctly represented in the database.
 */

const BASE_URL = "http://mochi.test:8888/browser/browser/components/places/tests/browser";
const PAGE_URL = BASE_URL + "/framedPage.html";
const LEFT_URL = BASE_URL + "/frameLeft.html";
const RIGHT_URL = BASE_URL + "/frameRight.html";

add_task(function* test() {
  // We must wait for both frames to be loaded and the visits to be registered.
  let deferredLeftFrameVisit = PromiseUtils.defer();
  let deferredRightFrameVisit = PromiseUtils.defer();

  Services.obs.addObserver(function observe(subject) {
    Task.spawn(function* () {
      let url = subject.QueryInterface(Ci.nsIURI).spec;
      if (url == LEFT_URL ) {
        is((yield getTransitionForUrl(url)), null,
           "Embed visits should not get a database entry.");
        deferredLeftFrameVisit.resolve();
      }
      else if (url == RIGHT_URL ) {
        is((yield getTransitionForUrl(url)),
           PlacesUtils.history.TRANSITION_FRAMED_LINK,
           "User activated visits should get a FRAMED_LINK transition.");
        Services.obs.removeObserver(observe, "uri-visit-saved");
        deferredRightFrameVisit.resolve();
      }
    });
  }, "uri-visit-saved", false);

  let tab = gBrowser.selectedTab = gBrowser.addTab(PAGE_URL);
  // Wait for all the subframes loads.
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser, true);
  // wait for the left frame visit to be registered;
  yield deferredLeftFrameVisit.promise;

  // Click on the link in the left frame to cause a page load in the
  // right frame.
  yield BrowserTestUtils.synthesizeMouseAtCenter(
    () => content.frames[0].document.getElementById("clickme"), {}, tab.linkedBrowser);

  // wait for the right frame visit to be registered;
  yield deferredRightFrameVisit.promise;

  yield BrowserTestUtils.removeTab(tab);
});

function* getTransitionForUrl(url) {
  let db = yield PlacesUtils.promiseDBConnection();
  let rows = yield db.execute(`
    SELECT visit_type
    FROM moz_historyvisits
    WHERE place_id = (SELECT id FROM moz_places WHERE url = :url)`,
    { url });
  if (rows.length) {
    return rows[0].getResultByName("visit_type");
  }
  return null;
}
