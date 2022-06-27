/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// End-to-end browser smoke test for Merino sessions. More comprehensive tests
// are in test_quicksuggest_merinoSessions.js. This test essentially makes sure
// engagements occur as expected when interacting with the urlbar. If you need
// to add tests that do not depend on a new definition of "engagement", consider
// adding them to test_quicksuggest_merinoSessions.js instead.

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

const { MERINO_PARAMS } = UrlbarProviderQuickSuggest;

// We set the Merino timeout to a large value to avoid intermittent failures in
// CI, especially TV tests, where the Merino fetch unexpectedly doesn't finish
// before the default timeout.
const TEST_MERINO_TIMEOUT_MS = 30000;

const MERINO_RESPONSE = {
  request_id: "request_id",
  suggestions: [
    {
      full_keyword: "full_keyword",
      title: "title",
      url: "url",
      icon: null,
      impression_url: "impression_url",
      click_url: "click_url",
      block_id: 1,
      advertiser: "advertiser",
      is_sponsored: true,
      score: 1,
    },
  ],
};

let gMerinoHandler;

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.merino.enabled", true],
      ["browser.urlbar.quicksuggest.remoteSettings.enabled", false],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
      ["browser.urlbar.merino.timeoutMs", TEST_MERINO_TIMEOUT_MS],
    ],
  });

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  // Install a mock default engine so we don't hit the network.
  await SearchTestUtils.installSearchExtension();
  let originalEngine = await Services.search.getDefault();
  let engine = Services.search.getEngineByName("Example");
  await Services.search.setDefault(engine);

  // Set up the mock Merino server.
  let path = "/merino";
  let server = makeMerinoServer(path);
  server = makeMerinoServer(path);
  server.start(-1);

  // Set up the mock endpoint URL.
  let url = new URL("http://localhost/");
  url.pathname = path;
  url.port = server.identity.primaryPort;
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.merino.endpointURL", url.toString()]],
  });

  registerCleanupFunction(async () => {
    server.stop();
    gMerinoHandler = null;
    await Services.search.setDefault(originalEngine);
  });
});

// In a single engagement, all requests should use the same session ID and the
// sequence number should be incremented.
add_task(async function singleEngagement() {
  let requests = [];
  setMerinoResponse(req => {
    requests.push({ params: new URLSearchParams(req.queryString) });
    return MERINO_RESPONSE;
  });

  for (let i = 0; i < 3; i++) {
    let searchString = "search" + i;
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
      fireInputEvent: true,
    });

    checkRequests(requests, {
      count: i + 1,
      areSessionIDsUnique: false,
      params: {
        [MERINO_PARAMS.QUERY]: searchString,
        [MERINO_PARAMS.SEQUENCE_NUMBER]: i,
      },
    });
  }

  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
});

// In a single engagement, all requests should use the same session ID and the
// sequence number should be incremented. This task closes the panel between
// searches but keeps the input focused, so the engagement should not end.
add_task(async function singleEngagement_panelClosed() {
  let requests = [];
  setMerinoResponse(req => {
    requests.push({ params: new URLSearchParams(req.queryString) });
    return MERINO_RESPONSE;
  });

  for (let i = 0; i < 3; i++) {
    let searchString = "search" + i;
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
      fireInputEvent: true,
    });

    checkRequests(requests, {
      count: i + 1,
      areSessionIDsUnique: false,
      params: {
        [MERINO_PARAMS.QUERY]: searchString,
        [MERINO_PARAMS.SEQUENCE_NUMBER]: i,
      },
    });

    EventUtils.synthesizeKey("KEY_Escape");
    Assert.ok(!UrlbarTestUtils.isPopupOpen(window), "Panel is closed");
    Assert.ok(gURLBar.focused, "Input remains focused");
  }

  // End the engagement to reset the session for the next test.
  gURLBar.blur();
});

// New engagements should not use the same session ID as previous engagements
// and the sequence number should be reset. This task completes each engagement
// successfully.
add_task(async function manyEngagements_engagement() {
  let requests = [];
  setMerinoResponse(req => {
    requests.push({ params: new URLSearchParams(req.queryString) });
    return MERINO_RESPONSE;
  });

  for (let i = 0; i < 3; i++) {
    // Open a new tab since we'll load the mock default search engine page.
    await BrowserTestUtils.withNewTab("about:blank", async () => {
      let searchString = "search" + i;
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: searchString,
        fireInputEvent: true,
      });

      checkRequests(requests, {
        count: i + 1,
        areSessionIDsUnique: true,
        params: {
          [MERINO_PARAMS.QUERY]: searchString,
          [MERINO_PARAMS.SEQUENCE_NUMBER]: 0,
        },
      });

      // Press enter on the heuristic result to load the search engine page and
      // complete the engagement.
      let loadPromise = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser
      );
      EventUtils.synthesizeKey("KEY_Enter");
      await loadPromise;
    });
  }
});

// New engagements should not use the same session ID as previous engagements
// and the sequence number should be reset. This task abandons each engagement.
add_task(async function manyEngagements_abandonment() {
  let requests = [];
  setMerinoResponse(req => {
    requests.push({ params: new URLSearchParams(req.queryString) });
    return MERINO_RESPONSE;
  });

  for (let i = 0; i < 3; i++) {
    let searchString = "search" + i;
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
      fireInputEvent: true,
    });

    checkRequests(requests, {
      count: i + 1,
      areSessionIDsUnique: true,
      params: {
        [MERINO_PARAMS.QUERY]: searchString,
        [MERINO_PARAMS.SEQUENCE_NUMBER]: 0,
      },
    });

    // Blur the urlbar to abandon the engagement.
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  }
});

/**
 * Asserts a given number of requests have been received and checks the last
 * request's search params.
 *
 * @param {array} requests
 *   An array of objects that look like `{ params }`.
 * @param {number} count
 *   The expected number of requests.
 * @param {object} params
 *   The expected search params of the last request. An object that maps param
 *   names to expected values.
 * @param {boolean} areSessionIDsUnique
 *   Whether each session ID param is expected to be unique.
 */
function checkRequests(requests, { count, params, areSessionIDsUnique }) {
  // Check the request count.
  Assert.equal(
    requests.length,
    count,
    "Expected request count for the current search"
  );

  // Check search params.
  let request = requests[count - 1];
  Assert.equal(
    request.params.get(MERINO_PARAMS.QUERY),
    params[MERINO_PARAMS.QUERY],
    "Query is correct"
  );

  let sessionID = request.params.get(MERINO_PARAMS.SESSION_ID);
  Assert.ok(sessionID, "Session ID was specified");
  Assert.ok(
    /^\{[0-9a-f]{8}-([0-9a-f]{4}-){3}[0-9a-f]{12}\}$/i.test(sessionID),
    "Session ID is a UUID"
  );

  let sequenceNumber = request.params.get(MERINO_PARAMS.SEQUENCE_NUMBER);
  Assert.ok(sequenceNumber, "Sequence number was specified");
  Assert.equal(
    parseInt(sequenceNumber),
    params[MERINO_PARAMS.SEQUENCE_NUMBER],
    "Sequence number is correct"
  );

  // Check the uniqueness of the session ID.
  for (let i = 0; i < count - 1; i++) {
    if (areSessionIDsUnique) {
      Assert.notEqual(
        request.params.get(MERINO_PARAMS.SESSION_ID),
        requests[i].params.get(MERINO_PARAMS.SESSION_ID),
        `Session ID is unique (comparing to index ${i})`
      );
    } else {
      Assert.equal(
        request.params.get(MERINO_PARAMS.SESSION_ID),
        requests[i].params.get(MERINO_PARAMS.SESSION_ID),
        `Session ID is same (comparing to index ${i})`
      );
    }
  }
}

function setMerinoResponse(callback) {
  gMerinoHandler = callback;
}

function makeMerinoServer(endpointPath) {
  let server = new HttpServer();
  server.registerPathHandler(endpointPath, async (req, resp) => {
    resp.processAsync();
    let merinoResponse = await gMerinoHandler(req);
    resp.setHeader("Content-Type", "application/json", false);
    resp.write(JSON.stringify(merinoResponse));
    resp.finish();
  });
  return server;
}
