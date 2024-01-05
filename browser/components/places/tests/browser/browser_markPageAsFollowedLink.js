/**
 * Tests that visits across frames are correctly represented in the database.
 */

const BASE_URL =
  "http://mochi.test:8888/browser/browser/components/places/tests/browser";
const PAGE_URL = BASE_URL + "/framedPage.html";
const LEFT_URL = BASE_URL + "/frameLeft.html";
const RIGHT_URL = BASE_URL + "/frameRight.html";

add_task(async function test() {
  // We must wait for both frames to be loaded and the visits to be registered.
  let deferredLeftFrameVisit = Promise.withResolvers();
  let deferredRightFrameVisit = Promise.withResolvers();

  Services.obs.addObserver(function observe(subject) {
    (async function () {
      let url = subject.QueryInterface(Ci.nsIURI).spec;
      if (url == LEFT_URL) {
        is(
          await getTransitionForUrl(url),
          null,
          "Embed visits should not get a database entry."
        );
        deferredLeftFrameVisit.resolve();
      } else if (url == RIGHT_URL) {
        is(
          await getTransitionForUrl(url),
          PlacesUtils.history.TRANSITION_FRAMED_LINK,
          "User activated visits should get a FRAMED_LINK transition."
        );
        Services.obs.removeObserver(observe, "uri-visit-saved");
        deferredRightFrameVisit.resolve();
      }
    })();
  }, "uri-visit-saved");

  // Open a tab and wait for all the subframes to load.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);

  // Wait for the left frame visit to be registered.
  info("Waiting left frame visit");
  await deferredLeftFrameVisit.promise;

  // Click on the link in the left frame to cause a page load in the
  // right frame.
  info("Clicking link");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    content.frames[0].document.getElementById("clickme").click();
  });

  // Wait for the right frame visit to be registered.
  info("Waiting right frame visit");
  await deferredRightFrameVisit.promise;

  BrowserTestUtils.removeTab(tab);
});

function getTransitionForUrl(url) {
  return PlacesUtils.withConnectionWrapper(
    "browser_markPageAsFollowedLink",
    async db => {
      let rows = await db.execute(
        `
      SELECT visit_type
      FROM moz_historyvisits
      JOIN moz_places h ON place_id = h.id
      WHERE url_hash = hash(:url) AND url = :url
      `,
        { url }
      );
      return rows.length ? rows[0].getResultByName("visit_type") : null;
    }
  );
}
