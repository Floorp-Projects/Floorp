/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test for MerinoClient sessions.

"use strict";

const { MerinoClient } = ChromeUtils.importESModule(
  "resource:///modules/MerinoClient.sys.mjs"
);

const { SEARCH_PARAMS } = MerinoClient;

let gClient;

add_setup(async () => {
  gClient = new MerinoClient();
  await MerinoTestUtils.server.start();
});

// In a single session, all requests should use the same session ID and the
// sequence number should be incremented.
add_task(async function singleSession() {
  for (let i = 0; i < 3; i++) {
    let query = "search" + i;
    await gClient.fetch({ query });

    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: i,
        },
      },
    ]);
  }

  gClient.resetSession();
});

// Different sessions should use different session IDs and the sequence number
// should be reset.
add_task(async function manySessions() {
  for (let i = 0; i < 3; i++) {
    let query = "search" + i;
    await gClient.fetch({ query });

    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: 0,
        },
      },
    ]);

    gClient.resetSession();
  }
});

// Tests two consecutive fetches:
//
// 1. Start a fetch
// 2. Wait for the mock Merino server to receive the request
// 3. Start a second fetch before the client receives the response
//
// The first fetch will be canceled by the second but the sequence number in the
// second fetch should still be incremented.
add_task(async function twoFetches_wait() {
  for (let i = 0; i < 3; i++) {
    // Send the first response after a delay to make sure the client will not
    // receive it before we start the second fetch.
    MerinoTestUtils.server.response.delay = UrlbarPrefs.get("merino.timeoutMs");

    // Start the first fetch but don't wait for it to finish.
    let requestPromise = MerinoTestUtils.server.waitForNextRequest();
    let query1 = "search" + i;
    gClient.fetch({ query: query1 });

    // Wait until the first request is received before starting the second
    // fetch, which will cancel the first. The response doesn't need to be
    // delayed, so remove it to make the test run faster.
    await requestPromise;
    delete MerinoTestUtils.server.response.delay;
    let query2 = query1 + "again";
    await gClient.fetch({ query: query2 });

    // The sequence number should have been incremented for each fetch.
    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query1,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i,
        },
      },
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query2,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i + 1,
        },
      },
    ]);
  }

  gClient.resetSession();
});

// Tests two consecutive fetches:
//
// 1. Start a fetch
// 2. Immediately start a second fetch
//
// The first fetch will be canceled by the second but the sequence number in the
// second fetch should still be incremented.
add_task(async function twoFetches_immediate() {
  for (let i = 0; i < 3; i++) {
    // Send the first response after a delay to make sure the client will not
    // receive it before we start the second fetch.
    MerinoTestUtils.server.response.delay =
      100 * UrlbarPrefs.get("merino.timeoutMs");

    // Start the first fetch but don't wait for it to finish.
    let query1 = "search" + i;
    gClient.fetch({ query: query1 });

    // Immediately do a second fetch that cancels the first. The response
    // doesn't need to be delayed, so remove it to make the test run faster.
    delete MerinoTestUtils.server.response.delay;
    let query2 = query1 + "again";
    await gClient.fetch({ query: query2 });

    // The sequence number should have been incremented for each fetch, but the
    // first won't have reached the server since it was immediately canceled.
    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query2,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i + 1,
        },
      },
    ]);
  }

  gClient.resetSession();
});

// When a network error occurs, the sequence number should still be incremented.
add_task(async function networkError() {
  for (let i = 0; i < 3; i++) {
    // Do a fetch that fails with a network error.
    let query1 = "search" + i;
    await MerinoTestUtils.server.withNetworkError(async () => {
      await gClient.fetch({ query: query1 });
    });

    Assert.equal(
      gClient.lastFetchStatus,
      "network_error",
      "The request failed with a network error"
    );

    // Do another fetch that successfully finishes.
    let query2 = query1 + "again";
    await gClient.fetch({ query: query2 });

    Assert.equal(
      gClient.lastFetchStatus,
      "success",
      "The request completed successfully"
    );

    // Only the second request should have been received but the sequence number
    // should have been incremented for each.
    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query2,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i + 1,
        },
      },
    ]);
  }

  gClient.resetSession();
});

// When the server returns a response with an HTTP error, the sequence number
// should be incremented.
add_task(async function httpError() {
  for (let i = 0; i < 3; i++) {
    // Do a fetch that fails with an HTTP error.
    MerinoTestUtils.server.response.status = 500;
    let query1 = "search" + i;
    await gClient.fetch({ query: query1 });

    Assert.equal(
      gClient.lastFetchStatus,
      "http_error",
      "The last request failed with a network error"
    );

    // Do another fetch that successfully finishes.
    MerinoTestUtils.server.response.status = 200;
    let query2 = query1 + "again";
    await gClient.fetch({ query: query2 });

    Assert.equal(
      gClient.lastFetchStatus,
      "success",
      "The last request completed successfully"
    );

    // Both requests should have been received and the sequence number should
    // have been incremented for each.
    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query1,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i,
        },
      },
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query2,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i + 1,
        },
      },
    ]);

    MerinoTestUtils.server.reset();
  }

  gClient.resetSession();
});

// When the client times out waiting for a response but later receives it and no
// other fetch happens in the meantime, the sequence number should be
// incremented.
add_task(async function clientTimeout_wait() {
  for (let i = 0; i < 3; i++) {
    // Do a fetch that causes the client to time out.
    MerinoTestUtils.server.response.delay =
      2 * UrlbarPrefs.get("merino.timeoutMs");
    let responsePromise = gClient.waitForNextResponse();
    let query1 = "search" + i;
    await gClient.fetch({ query: query1 });

    Assert.equal(
      gClient.lastFetchStatus,
      "timeout",
      "The last request failed with a client timeout"
    );

    // Wait for the client to receive the response.
    await responsePromise;

    // Do another fetch that successfully finishes.
    delete MerinoTestUtils.server.response.delay;
    let query2 = query1 + "again";
    await gClient.fetch({ query: query2 });

    Assert.equal(
      gClient.lastFetchStatus,
      "success",
      "The last request completed successfully"
    );

    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query1,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i,
        },
      },
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query2,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i + 1,
        },
      },
    ]);
  }

  gClient.resetSession();
});

// When the client times out waiting for a response and a second fetch starts
// before the response is received, the first fetch should be canceled but the
// sequence number should still be incremented.
add_task(async function clientTimeout_canceled() {
  for (let i = 0; i < 3; i++) {
    // Do a fetch that causes the client to time out.
    MerinoTestUtils.server.response.delay =
      2 * UrlbarPrefs.get("merino.timeoutMs");
    let query1 = "search" + i;
    await gClient.fetch({ query: query1 });

    Assert.equal(
      gClient.lastFetchStatus,
      "timeout",
      "The last request failed with a client timeout"
    );

    // Do another fetch that successfully finishes.
    delete MerinoTestUtils.server.response.delay;
    let query2 = query1 + "again";
    await gClient.fetch({ query: query2 });

    Assert.equal(
      gClient.lastFetchStatus,
      "success",
      "The last request completed successfully"
    );

    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query1,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i,
        },
      },
      {
        params: {
          [SEARCH_PARAMS.QUERY]: query2,
          [SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i + 1,
        },
      },
    ]);
  }

  gClient.resetSession();
});

// When the session times out, the next fetch should use a new session ID and
// the sequence number should be reset.
add_task(async function sessionTimeout() {
  // Set the session timeout to something reasonable to test.
  let originalTimeoutMs = gClient.sessionTimeoutMs;
  gClient.sessionTimeoutMs = 500;

  // Do a fetch.
  let query1 = "search";
  await gClient.fetch({ query: query1 });

  // Wait for the session to time out.
  await gClient.waitForNextSessionReset();

  Assert.strictEqual(
    gClient.sessionID,
    null,
    "sessionID is null after session timeout"
  );
  Assert.strictEqual(
    gClient.sequenceNumber,
    0,
    "sequenceNumber is zero after session timeout"
  );
  Assert.strictEqual(
    gClient._test_sessionTimer,
    null,
    "sessionTimer is null after session timeout"
  );

  // Do another fetch.
  let query2 = query1 + "again";
  await gClient.fetch({ query: query2 });

  // The second request's sequence number should be zero due to the session
  // timeout.
  MerinoTestUtils.server.checkAndClearRequests([
    {
      params: {
        [SEARCH_PARAMS.QUERY]: query1,
        [SEARCH_PARAMS.SEQUENCE_NUMBER]: 0,
      },
    },
    {
      params: {
        [SEARCH_PARAMS.QUERY]: query2,
        [SEARCH_PARAMS.SEQUENCE_NUMBER]: 0,
      },
    },
  ]);

  Assert.ok(
    gClient.sessionID,
    "sessionID is non-null after first request in a new session"
  );
  Assert.equal(
    gClient.sequenceNumber,
    1,
    "sequenceNumber is one after first request in a new session"
  );
  Assert.ok(
    gClient._test_sessionTimer,
    "sessionTimer is non-null after first request in a new session"
  );

  gClient.sessionTimeoutMs = originalTimeoutMs;
  gClient.resetSession();
});
