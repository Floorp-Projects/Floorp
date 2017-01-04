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
      } else if (url == RIGHT_URL ) {
        is((yield getTransitionForUrl(url)),
           PlacesUtils.history.TRANSITION_FRAMED_LINK,
           "User activated visits should get a FRAMED_LINK transition.");
        Services.obs.removeObserver(observe, "uri-visit-saved");
        deferredRightFrameVisit.resolve();
      }
    });
  }, "uri-visit-saved", false);

  // Open a tab and wait for all the subframes to load.
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);

  // Wait for the left frame visit to be registered.
  info("Waiting left frame visit");
  yield deferredLeftFrameVisit.promise;

  // Click on the link in the left frame to cause a page load in the
  // right frame.
  info("Clicking link");
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.frames[0].document.getElementById("clickme").click();
  });

  // Wait for the right frame visit to be registered.
  info("Waiting right frame visit");
  yield deferredRightFrameVisit.promise;

  yield BrowserTestUtils.removeTab(tab);
});

function* getTransitionForUrl(url) {
  // Ensure all the transactions completed.
  yield PlacesTestUtils.promiseAsyncUpdates();
  let db = yield PlacesUtils.promiseDBConnection();
  let rows = yield db.execute(`
    SELECT visit_type
    FROM moz_historyvisits
    JOIN moz_places h ON place_id = h.id
    WHERE url_hash = hash(:url) AND url = :url`,
    { url });
  if (rows.length) {
    return rows[0].getResultByName("visit_type");
  }
  return null;
}
