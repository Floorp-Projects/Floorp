/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test for Merino sessions.

"use strict";

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

// We set the Merino timeout to a large value to avoid intermittent failures in
// CI, especially TV tests, where the Merino fetch unexpectedly doesn't finish
// before the default timeout.
const TEST_MERINO_TIMEOUT_MS = 1000;

const { MERINO_PARAMS } = UrlbarProviderQuickSuggest;

const MERINO_RESPONSE = {
  contentType: "application/json",
  body: {
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
  },
};

let gMerinoResponse;

add_task(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("merino.enabled", true);
  UrlbarPrefs.set("quicksuggest.remoteSettings.enabled", false);
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);

  // Set up the Merino server.
  let path = "/merino";
  let server = makeMerinoServer(path);
  let url = new URL("http://localhost/");
  url.pathname = path;
  url.port = server.identity.primaryPort;
  UrlbarPrefs.set("merino.endpointURL", url.toString());
  UrlbarPrefs.set("merino.timeoutMs", TEST_MERINO_TIMEOUT_MS);
});

// In a single engagement, all requests should use the same session ID and the
// sequence number should be incremented.
add_task(async function singleEngagement() {
  let requests = [];
  setMerinoResponse(req => {
    requests.push({ params: new URLSearchParams(req.queryString) });
    return MERINO_RESPONSE;
  });

  let controller = UrlbarTestUtils.newMockController();

  for (let i = 0; i < 3; i++) {
    let searchString = "search" + i;
    await controller.startQuery(
      createContext(searchString, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      })
    );

    checkRequests(requests, {
      count: i + 1,
      areSessionIDsUnique: false,
      params: {
        [MERINO_PARAMS.QUERY]: searchString,
        [MERINO_PARAMS.SEQUENCE_NUMBER]: i,
      },
    });
  }

  // End the engagement to reset the session for the next test.
  endEngagement();
});

// New engagements should not use the same session ID as previous engagements
// and the sequence number should be reset. This task completes each engagement
// successfully.
add_task(async function manyEngagements_engagement() {
  await doManyEngagementsTest("engagement");
});

// New engagements should not use the same session ID as previous engagements
// and the sequence number should be reset. This task abandons each engagement.
add_task(async function manyEngagements_abandonment() {
  await doManyEngagementsTest("abandonment");
});

async function doManyEngagementsTest(state) {
  let requests = [];
  setMerinoResponse(req => {
    requests.push({ params: new URLSearchParams(req.queryString) });
    return MERINO_RESPONSE;
  });

  let controller = UrlbarTestUtils.newMockController();

  for (let i = 0; i < 3; i++) {
    let searchString = "search" + i;
    let context = createContext(searchString, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    });
    await controller.startQuery(context);

    checkRequests(requests, {
      count: i + 1,
      areSessionIDsUnique: true,
      params: {
        [MERINO_PARAMS.QUERY]: searchString,
        [MERINO_PARAMS.SEQUENCE_NUMBER]: 0,
      },
    });

    endEngagement(context, state);
  }
}

// When a search is canceled before the Merino response is received, the
// sequence number should not be incremented.
add_task(async function canceledQueries() {
  let requests = [];
  setMerinoResponse(req => {
    requests.push({ params: new URLSearchParams(req.queryString) });
    return MERINO_RESPONSE;
  });

  let controller = UrlbarTestUtils.newMockController();

  for (let i = 0; i < 3; i++) {
    // Start a search but don't wait for it to finish.
    controller.startQuery(
      createContext("search" + i, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      })
    );

    // Do another search that cancels the first.
    let searchString = "search" + i + "again";
    let context = createContext(searchString, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    });
    await controller.startQuery(context);

    // Only one request should have been received and the sequence number should
    // be incremented only once compared to the previous successful search.
    checkRequests(requests, {
      count: i + 1,
      areSessionIDsUnique: false,
      params: {
        [MERINO_PARAMS.QUERY]: searchString,
        [MERINO_PARAMS.SEQUENCE_NUMBER]: i,
      },
    });
  }

  // End the engagement to reset the session for the next test.
  endEngagement();
});

// When a network error occurs, the sequence number should not be incremented.
add_task(async function networkError() {
  let requests = [];
  setMerinoResponse(req => {
    requests.push({ params: new URLSearchParams(req.queryString) });
    return MERINO_RESPONSE;
  });

  let controller = UrlbarTestUtils.newMockController();

  for (let i = 0; i < 3; i++) {
    // Do a search that fails with a network error.
    await withNetworkError(async () => {
      await controller.startQuery(
        createContext("search" + i, {
          providers: [UrlbarProviderQuickSuggest.name],
          isPrivate: false,
        })
      );
    });

    // Do another search that successfully finishes.
    let searchString = "search" + i + "again";
    await controller.startQuery(
      createContext(searchString, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      })
    );

    // Only one request should have been received and the sequence number should
    // be incremented only once compared to the previous successful search.
    checkRequests(requests, {
      count: i + 1,
      areSessionIDsUnique: false,
      params: {
        [MERINO_PARAMS.QUERY]: searchString,
        [MERINO_PARAMS.SEQUENCE_NUMBER]: i,
      },
    });
  }

  // End the engagement to reset the session for the next test.
  endEngagement();
});

// When the server returns a response with an HTTP error, the sequence number
// should be incremented.
add_task(async function httpError() {
  let status;
  let requests = [];
  setMerinoResponse(req => {
    requests.push({ status, params: new URLSearchParams(req.queryString) });
    let resp = deepCopy(MERINO_RESPONSE);
    resp.status = status;
    return resp;
  });

  let controller = UrlbarTestUtils.newMockController();

  for (let i = 0; i < 6; i++) {
    status = i % 2 ? 200 : 500;

    let searchString = "search" + i;
    await controller.startQuery(
      createContext(searchString, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      })
    );

    Assert.equal(
      requests[i].status,
      status,
      "The request was answered with the expected status"
    );

    checkRequests(requests, {
      count: i + 1,
      areSessionIDsUnique: false,
      params: {
        [MERINO_PARAMS.QUERY]: searchString,
        [MERINO_PARAMS.SEQUENCE_NUMBER]: i,
      },
    });
  }

  // End the engagement to reset the session for the next test.
  endEngagement();
});

// When the server "times out" but returns a response and no other search
// happens in the meantime, the sequence number should be incremented.
add_task(async function merinoTimeout_wait() {
  let delay = 0;
  let requests = [];
  setMerinoResponse(req => {
    requests.push({ delay, params: new URLSearchParams(req.queryString) });
    let resp = deepCopy(MERINO_RESPONSE);
    resp.delay = delay;
    return resp;
  });

  let controller = UrlbarTestUtils.newMockController();

  for (let i = 0; i < 2; i++) {
    // Don't delay the response to the first search. Delay the response to the
    // second search enough that it times out.
    delay = i == 1 ? 2 * UrlbarPrefs.get("merinoTimeoutMs") : 0;

    let searchString = "search" + i;
    await controller.startQuery(
      createContext(searchString, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      })
    );

    Assert.equal(
      requests[i].delay,
      delay,
      "The request was answered with the expected delay"
    );

    checkRequests(requests, {
      count: i + 1,
      areSessionIDsUnique: false,
      params: {
        [MERINO_PARAMS.QUERY]: searchString,
        [MERINO_PARAMS.SEQUENCE_NUMBER]: i,
      },
    });
  }

  // End the engagement to reset the session for the next test.
  endEngagement();
});

// When the server "times out" and a second search starts before the first
// search receives a response, the first search should be canceled and the
// sequence number should not be incremented.
add_task(async function merinoTimeout_canceled() {
  let delay = 0;
  let requests = [];
  setMerinoResponse(req => {
    requests.push({ delay, params: new URLSearchParams(req.queryString) });
    let resp = deepCopy(MERINO_RESPONSE);
    resp.delay = delay;
    return resp;
  });

  let controller = UrlbarTestUtils.newMockController();

  for (let i = 0; i < 3; i++) {
    // Don't delay the response to the first and third searches. Delay the
    // response to the second search enough that it times out.
    delay = i == 1 ? 3 * UrlbarPrefs.get("merinoTimeoutMs") : 0;

    let searchString = "search" + i;
    let queryPromise = controller.startQuery(
      createContext(searchString, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      })
    );

    if (i == 1) {
      // For the second search, wait long enough for it to time out but not so
      // long that it completes, and then continue to the third search, which
      // should cancel it.
      await new Promise(resolve =>
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        setTimeout(resolve, 2 * UrlbarPrefs.get("merinoTimeoutMs"))
      );
      continue;
    }

    await queryPromise;

    Assert.equal(
      requests[i].delay,
      delay,
      "The request was answered with the expected delay"
    );

    checkRequests(requests, {
      count: i + 1,
      areSessionIDsUnique: false,
      params: {
        [MERINO_PARAMS.QUERY]: searchString,
        [MERINO_PARAMS.SEQUENCE_NUMBER]: i == 0 ? 0 : 1,
      },
    });
  }

  // End the engagement to reset the session for the next test.
  endEngagement();
});

// When the session times out due to no engagement, the next search should use a
// new session ID and the sequence number should be reset.
add_task(async function sessionTimeout() {
  // Set the session timeout to something reasonable to test.
  let timeoutMs = 500;
  let originalTimeoutMs = UrlbarProviderQuickSuggest._merinoSessionTimeoutMs;
  UrlbarProviderQuickSuggest._merinoSessionTimeoutMs = timeoutMs;

  let requests = [];
  setMerinoResponse(req => {
    requests.push({ params: new URLSearchParams(req.queryString) });
    return MERINO_RESPONSE;
  });

  let controller = UrlbarTestUtils.newMockController();

  // Do a search.
  let context = createContext("search 0", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await controller.startQuery(context);

  checkRequests(requests, {
    count: 1,
    areSessionIDsUnique: true,
    params: {
      [MERINO_PARAMS.QUERY]: "search 0",
      [MERINO_PARAMS.SEQUENCE_NUMBER]: 0,
    },
  });

  // Wait for the session to time out.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 2 * timeoutMs));

  Assert.strictEqual(
    UrlbarProviderQuickSuggest._merinoSessionID,
    null,
    "_merinoSessionID is null after session timeout"
  );
  Assert.strictEqual(
    UrlbarProviderQuickSuggest._merinoSessionTimer,
    null,
    "_merinoSessionTimer is null after session timeout"
  );

  // Do another search.
  await controller.startQuery(
    createContext("search 1", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    })
  );

  checkRequests(requests, {
    count: 2,
    areSessionIDsUnique: true,
    params: {
      [MERINO_PARAMS.QUERY]: "search 1",
      [MERINO_PARAMS.SEQUENCE_NUMBER]: 0,
    },
  });

  UrlbarProviderQuickSuggest._merinoSessionTimeoutMs = originalTimeoutMs;

  // End the engagement to reset the session for the next test.
  endEngagement();
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

  // Check last request's search params.
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

  // Check the uniqueness of the last request's session ID.
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

function endEngagement(context = null, state = "engagement") {
  UrlbarProviderQuickSuggest.onEngagement(
    false,
    state,
    context ||
      createContext("endEngagement", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
    { selIndex: -1 }
  );

  Assert.strictEqual(
    UrlbarProviderQuickSuggest._merinoSessionID,
    null,
    "_merinoSessionID is null after engagement"
  );
  Assert.strictEqual(
    UrlbarProviderQuickSuggest._merinoSessionTimer,
    null,
    "_merinoSessionTimer is null after engagement"
  );
}

async function withNetworkError(callback) {
  // Set the endpoint to a valid, unreachable URL.
  let originalURL = UrlbarPrefs.get("merino.endpointURL");
  UrlbarPrefs.set(
    "merino.endpointURL",
    "http://localhost/test_quicksuggest_merinoSessions"
  );

  // Set the timeout high enough that the network error exception will happen
  // first. On Mac and Linux the fetch naturally times out fairly quickly but on
  // Windows it seems to take 5s, so set our artificial timeout to 10s.
  UrlbarPrefs.set("merino.timeoutMs", 10000);

  await callback();

  UrlbarPrefs.set("merino.endpointURL", originalURL);
  UrlbarPrefs.set("merino.timeoutMs", TEST_MERINO_TIMEOUT_MS);
}

function setMerinoResponse(resp = MERINO_RESPONSE) {
  if (typeof resp == "function") {
    info("Setting Merino response to a callback function");
    gMerinoResponse = resp;
  } else {
    info("Setting Merino response: " + JSON.stringify(resp));
    gMerinoResponse = deepCopy(resp);
  }
  return gMerinoResponse;
}

function makeMerinoServer(endpointPath) {
  let server = makeTestServer();
  server.registerPathHandler(endpointPath, async (req, resp) => {
    resp.processAsync();

    let merinoResponse;
    if (typeof gMerinoResponse == "function") {
      merinoResponse = await gMerinoResponse(req);
    } else {
      merinoResponse = gMerinoResponse;
    }

    if (typeof merinoResponse.delay == "number") {
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, merinoResponse.delay));
    }

    if (typeof merinoResponse.status == "number") {
      resp.setStatusLine("", merinoResponse.status, merinoResponse.status);
    }

    resp.setHeader("Content-Type", merinoResponse.contentType, false);

    if (typeof merinoResponse.body == "string") {
      resp.write(merinoResponse.body);
    } else if (merinoResponse.body) {
      resp.write(JSON.stringify(merinoResponse.body));
    }

    resp.finish();
  });

  return server;
}

function deepCopy(obj) {
  return JSON.parse(JSON.stringify(obj));
}
