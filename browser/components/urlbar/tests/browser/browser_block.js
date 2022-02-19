/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests whether block button works.

"use strict";

const MAX_RESULTS = UrlbarPrefs.get("maxRichResults");

add_task(async function setup() {
  // Add enough results to fill up the view.
  await PlacesUtils.history.clear();
  for (let i = 0; i < MAX_RESULTS; i++) {
    await PlacesTestUtils.addVisits("http://example.com/" + i);
  }

  // Setup for best match provider.
  const provider = new UrlbarTestUtils.TestProvider({
    results: [makeBestMatchResult()],
  });
  UrlbarProvidersManager.registerProvider(provider);

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    UrlbarProvidersManager.unregisterProvider(provider);
  });
});

add_task(async function blockBestMatch() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "example",
  });

  info("Check initial results");
  const bestMatchRow = await findBestMatchRow();
  Assert.ok(bestMatchRow, "The results has best match");
  const blockButton = bestMatchRow._buttons.get("block");
  Assert.ok(blockButton, "The best match eow has a block button");
  const initialURLs = await getResultsAsURL();
  const indexOfBestMatch = bestMatchRow.result.rowIndex;

  info("Click on the block button");
  EventUtils.synthesizeMouseAtCenter(blockButton, {});

  info("Check results after blocking best match");
  Assert.ok(!(await findBestMatchRow()), "The results does have best match");
  const currentURLs = await getResultsAsURL();
  Assert.ok(currentURLs.length, "The popup is still dislayed");
  Assert.equal(
    currentURLs.length,
    initialURLs.length - 1,
    "One row is removed from the results"
  );
  initialURLs.splice(indexOfBestMatch, 1);
  Assert.equal(
    JSON.stringify(currentURLs),
    JSON.stringify(initialURLs),
    "The order of results is correct"
  );

  await UrlbarTestUtils.promisePopupClose(window);
});

async function findBestMatchRow() {
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    const details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    const { row } = details.element;
    if (row.getAttribute("type") === "bestmatch") {
      return row;
    }
  }

  return null;
}

async function getResultsAsURL() {
  const urls = [];
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    const details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    urls.push(details.element.row.result.payload.url);
  }
  return urls;
}

function makeBestMatchResult(payloadExtra = {}) {
  return Object.assign(
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights([], {
        title: "Test best match",
        url: "https://example.com/best-match",
        ...payloadExtra,
      })
    ),
    { isBestMatch: true }
  );
}
