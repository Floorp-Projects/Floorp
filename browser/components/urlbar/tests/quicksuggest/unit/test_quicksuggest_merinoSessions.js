/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests Merino session integration with UrlbarProviderQuickSuggest.

"use strict";

// `UrlbarProviderQuickSuggest.#merino` is lazily created on the first Merino
// fetch, so it's easiest to create `gClient` lazily too.
ChromeUtils.defineLazyGetter(
  this,
  "gClient",
  () => UrlbarProviderQuickSuggest._test_merino
);

add_task(async function init() {
  await MerinoTestUtils.server.start();
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    prefs: [
      ["suggest.quicksuggest.sponsored", true],
      ["quicksuggest.dataCollection.enabled", true],
    ],
  });
});

// In a single engagement, all requests should use the same session ID and the
// sequence number should be incremented.
add_task(async function singleEngagement() {
  let controller = UrlbarTestUtils.newMockController();

  for (let i = 0; i < 3; i++) {
    let searchString = "search" + i;
    await controller.startQuery(
      createContext(searchString, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      })
    );

    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [MerinoTestUtils.SEARCH_PARAMS.QUERY]: searchString,
          [MerinoTestUtils.SEARCH_PARAMS.SEQUENCE_NUMBER]: i,
        },
      },
    ]);
  }

  // End the engagement to reset the session for the next test.
  endEngagement({ controller });
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
  let controller = UrlbarTestUtils.newMockController();

  for (let i = 0; i < 3; i++) {
    let searchString = "search" + i;
    let context = createContext(searchString, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    });
    await controller.startQuery(context);

    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [MerinoTestUtils.SEARCH_PARAMS.QUERY]: searchString,
          [MerinoTestUtils.SEARCH_PARAMS.SEQUENCE_NUMBER]: 0,
        },
      },
    ]);

    endEngagement({ context, state, controller });
  }
}

// When a search is canceled after the request is sent and before the Merino
// response is received, the sequence number should still be incremented.
add_task(async function canceledQueries() {
  let controller = UrlbarTestUtils.newMockController();

  for (let i = 0; i < 3; i++) {
    // Send the first response after a delay to make sure the client will not
    // receive it before we start the second fetch.
    MerinoTestUtils.server.response.delay = UrlbarPrefs.get("merino.timeoutMs");

    // Start the first search.
    let requestPromise = MerinoTestUtils.server.waitForNextRequest();
    let searchString1 = "search" + i;
    controller.startQuery(
      createContext(searchString1, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      })
    );

    // Wait until the first request is received before starting the second
    // search. If we started the second search immediately, the first would be
    // canceled before the provider is even called due to the urlbar's 50ms
    // delay (see `browser.urlbar.delay`) so the sequence number would not be
    // incremented for it. Here we want to test the case where the first search
    // is canceled after the request is sent and the number is incremented.
    await requestPromise;
    delete MerinoTestUtils.server.response.delay;

    // Now do a second search that cancels the first.
    let searchString2 = searchString1 + "again";
    await controller.startQuery(
      createContext(searchString2, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      })
    );

    // The sequence number should have been incremented for each search.
    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [MerinoTestUtils.SEARCH_PARAMS.QUERY]: searchString1,
          [MerinoTestUtils.SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i,
        },
      },
      {
        params: {
          [MerinoTestUtils.SEARCH_PARAMS.QUERY]: searchString2,
          [MerinoTestUtils.SEARCH_PARAMS.SEQUENCE_NUMBER]: 2 * i + 1,
        },
      },
    ]);
  }

  // End the engagement to reset the session for the next test.
  endEngagement({ controller });
});

function endEngagement({ controller, context = null, state = "engagement" }) {
  UrlbarProviderQuickSuggest.onEngagement(
    state,
    context ||
      createContext("endEngagement", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
    { selIndex: -1 },
    controller
  );

  Assert.strictEqual(
    gClient.sessionID,
    null,
    "sessionID is null after engagement"
  );
  Assert.strictEqual(
    gClient._test_sessionTimer,
    null,
    "sessionTimer is null after engagement"
  );
}
