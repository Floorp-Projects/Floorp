/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether a visit information is annotated correctly when picking a result.

const { VISIT_SOURCE_ORGANIC, VISIT_SOURCE_SPONSORED } = PlacesUtils.history;

async function assertDatabase({ targetURL, expected }) {
  const placesId = await PlacesTestUtils.fieldInDB(targetURL, "id");
  const expectedTriggeringPlaceId = expected.triggerURL
    ? await PlacesTestUtils.fieldInDB(expected.triggerURL, "id")
    : null;
  const db = await PlacesUtils.promiseDBConnection();
  const rows = await db.execute(
    "SELECT source, triggeringPlaceId FROM moz_historyvisits WHERE place_id = :place_id AND source = :source",
    {
      place_id: placesId,
      source: expected.source,
    }
  );
  Assert.equal(rows.length, 1);
  Assert.equal(
    rows[0].getResultByName("triggeringPlaceId"),
    expectedTriggeringPlaceId,
    `The triggeringPlaceId in database is correct for ${targetURL}`
  );
}

function registerProvider(payload) {
  const provider = new UrlbarTestUtils.TestProvider({
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.SEARCH,
        ...UrlbarResult.payloadAndSimpleHighlights([], {
          ...payload,
        })
      ),
    ],
    priority: Infinity,
  });
  UrlbarProvidersManager.registerProvider(provider);
  return provider;
}

async function pickResult({ input, payloadURL, redirectTo }) {
  const destinationURL = redirectTo || payloadURL;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: input,
    fireInputEvent: true,
  });

  const result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.url, payloadURL);
  UrlbarTestUtils.setSelectedRowIndex(window, 0);

  info("Show result and wait for loading");
  const onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    destinationURL
  );
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;
}

add_setup(async function() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function basic() {
  const testData = [
    {
      description: "Sponsored result",
      input: "exa",
      payload: {
        url: "http://example.com/",
        isSponsored: true,
      },
      expected: {
        source: VISIT_SOURCE_SPONSORED,
      },
    },
    {
      description: "Organic result",
      input: "exa",
      payload: {
        url: "http://example.com/",
      },
      expected: {
        source: VISIT_SOURCE_ORGANIC,
      },
    },
  ];

  for (const { description, input, payload, expected } of testData) {
    info(description);
    const provider = registerProvider(payload);

    await BrowserTestUtils.withNewTab("about:blank", async () => {
      info("Pick result");
      await pickResult({ input, payloadURL: payload.url });

      info("Check database");
      await assertDatabase({ targetURL: payload.url, expected });
    });

    UrlbarProvidersManager.unregisterProvider(provider);
    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();
  }
});

add_task(async function redirection() {
  const redirectTo = "http://example.com/";
  const payload = {
    url:
      "http://example.com/browser/browser/components/urlbar/tests/browser/redirect_to.sjs?/",
    isSponsored: true,
  };
  const input = "exa";
  const provider = registerProvider(payload);

  await BrowserTestUtils.withNewTab("about:home", async () => {
    info("Pick result");
    await pickResult({ input, payloadURL: payload.url, redirectTo });

    info("Check database");
    await assertDatabase({
      targetURL: payload.url,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
      },
    });
    await assertDatabase({
      targetURL: redirectTo,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        triggerURL: payload.url,
      },
    });
  });

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  UrlbarProvidersManager.unregisterProvider(provider);
});
