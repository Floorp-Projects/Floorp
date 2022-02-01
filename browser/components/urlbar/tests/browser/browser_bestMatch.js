/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests best match rows in the view.

"use strict";

// Tests a non-sponsored best match row.
add_task(async function nonsponsored() {
  let result = makeBestMatchResult();
  await withProvider(result, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkBestMatchRow({ result });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Tests a non-sponsored best match row with a help button.
add_task(async function nonsponsoredHelpButton() {
  let result = makeBestMatchResult({ helpUrl: "https://example.com/help" });
  await withProvider(result, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkBestMatchRow({ result, hasHelpButton: true });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Tests a sponsored best match row.
add_task(async function sponsored() {
  let result = makeBestMatchResult({ isSponsored: true });
  await withProvider(result, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkBestMatchRow({ result, isSponsored: true });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Tests a sponsored best match row with a help button.
add_task(async function sponsoredHelpButton() {
  let result = makeBestMatchResult({
    isSponsored: true,
    helpUrl: "https://example.com/help",
  });
  await withProvider(result, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkBestMatchRow({ result, isSponsored: true, hasHelpButton: true });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

async function checkBestMatchRow({
  result,
  isSponsored = false,
  hasHelpButton = false,
}) {
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "One result is present"
  );

  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  let { row } = details.element;

  Assert.equal(row.getAttribute("type"), "bestmatch", "row[type] is bestmatch");

  let favicon = row._elements.get("favicon");
  Assert.ok(favicon, "Row has a favicon");

  let title = row._elements.get("title");
  Assert.ok(title, "Row has a title");
  Assert.ok(title.textContent, "Row title has non-empty textContext");
  Assert.equal(title.textContent, result.payload.title, "Row title is correct");

  let url = row._elements.get("url");
  Assert.ok(url, "Row has a URL");
  Assert.ok(url.textContent, "Row URL has non-empty textContext");
  Assert.equal(
    url.textContent,
    result.payload.displayUrl,
    "Row URL is correct"
  );

  let bottom = row._elements.get("bottom");
  Assert.ok(bottom, "Row has a bottom");
  Assert.equal(
    !!result.payload.isSponsored,
    isSponsored,
    "Sanity check: Row's expected isSponsored matches result's"
  );
  if (isSponsored) {
    Assert.equal(
      bottom.textContent,
      "Sponsored",
      "Sponsored row bottom has Sponsored textContext"
    );
  } else {
    Assert.equal(
      bottom.textContent,
      "",
      "Non-sponsored row bottom has empty textContext"
    );
  }

  let helpButton = row._elements.get("helpButton");
  Assert.equal(
    !!result.payload.helpUrl,
    hasHelpButton,
    "Sanity check: Row's expected hasHelpButton matches result"
  );
  if (hasHelpButton) {
    Assert.ok(helpButton, "Row with helpUrl has a helpButton");
  } else {
    Assert.ok(!helpButton, "Row without helpUrl does not have a helpButton");
  }
}

async function withProvider(result, callback) {
  let provider = new UrlbarTestUtils.TestProvider({
    results: [result],
    priority: Infinity,
  });
  UrlbarProvidersManager.registerProvider(provider);
  try {
    await callback();
  } finally {
    UrlbarProvidersManager.unregisterProvider(provider);
  }
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
