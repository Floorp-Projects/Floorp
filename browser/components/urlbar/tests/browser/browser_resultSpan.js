/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that displaying results with resultSpan > 1 limits other results in
// the view.

const TEST_RESULTS = [
  new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    { url: "http://mozilla.org/1" }
  ),
  new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TIP,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    {
      text: "This is a test tip.",
      buttonText: "Done",
      type: "test",
      helpUrl: "about:about",
    }
  ),
];

const MAX_RESULTS = UrlbarPrefs.get("maxRichResults");
const TIP_SPAN = UrlbarUtils.getSpanForResult({
  type: UrlbarUtils.RESULT_TYPE.TIP,
});

add_task(async function init() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
});

// A restricting provider with one tip result and many history results.
add_task(async function oneTip() {
  let results = Array.from(TEST_RESULTS);
  for (let i = TEST_RESULTS.length; i < MAX_RESULTS; i++) {
    results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: `http://mozilla.org/${i}` }
      )
    );
  }

  let expectedResults = Array.from(results).slice(
    0,
    MAX_RESULTS - TIP_SPAN + 1
  );

  let provider = new UrlbarTestUtils.TestProvider({ results, priority: 1 });
  UrlbarProvidersManager.registerProvider(provider);

  let context = await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });

  checkResults(context.results, expectedResults);

  UrlbarProvidersManager.unregisterProvider(provider);
  gURLBar.view.close();
});

// A restricting provider with three tip results and many history results.
add_task(async function threeTips() {
  let results = Array.from(TEST_RESULTS);
  for (let i = 1; i < 3; i++) {
    results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.TIP,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          text: "This is a test tip.",
          buttonText: "Done",
          type: "test",
          helpUrl: `about:about#${i}`,
        }
      )
    );
  }
  for (let i = 2; i < 15; i++) {
    results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: `http://mozilla.org/${i}` }
      )
    );
  }

  let expectedResults = Array.from(results).slice(
    0,
    MAX_RESULTS - 3 * (TIP_SPAN - 1)
  );

  let provider = new UrlbarTestUtils.TestProvider({ results, priority: 1 });
  UrlbarProvidersManager.registerProvider(provider);

  let context = await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });

  checkResults(context.results, expectedResults);

  UrlbarProvidersManager.unregisterProvider(provider);
  gURLBar.view.close();
});

// A non-restricting provider with one tip results and many history results.
add_task(async function oneTip_nonRestricting() {
  let results = Array.from(TEST_RESULTS);
  for (let i = 2; i < 15; i++) {
    results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: `http://mozilla.org/${i}` }
      )
    );
  }

  let expectedResults = Array.from(results);

  // UnifiedComplete's heuristic search result
  expectedResults.unshift({
    type: UrlbarUtils.RESULT_TYPE.SEARCH,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    payload: {
      engine: Services.search.defaultEngine.name,
      query: "test",
    },
  });

  expectedResults = expectedResults.slice(0, MAX_RESULTS - TIP_SPAN + 1);

  let provider = new UrlbarTestUtils.TestProvider({ results });
  UrlbarProvidersManager.registerProvider(provider);

  let context = await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });

  checkResults(context.results, expectedResults);

  UrlbarProvidersManager.unregisterProvider(provider);
  gURLBar.view.close();
});

// A non-restricting provider with three tip results and many history results.
add_task(async function threeTips_nonRestricting() {
  let results = Array.from(TEST_RESULTS);
  for (let i = 1; i < 3; i++) {
    results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.TIP,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          text: "This is a test tip.",
          buttonText: "Done",
          type: "test",
          helpUrl: `about:about#${i}`,
        }
      )
    );
  }
  for (let i = 2; i < 15; i++) {
    results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: `http://mozilla.org/${i}` }
      )
    );
  }

  let expectedResults = Array.from(results);

  // UnifiedComplete's heuristic search result
  expectedResults.unshift({
    type: UrlbarUtils.RESULT_TYPE.SEARCH,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    payload: {
      engine: Services.search.defaultEngine.name,
      query: "test",
    },
  });

  expectedResults = expectedResults.slice(0, MAX_RESULTS - 3 * (TIP_SPAN - 1));

  let provider = new UrlbarTestUtils.TestProvider({ results });
  UrlbarProvidersManager.registerProvider(provider);

  let context = await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });

  checkResults(context.results, expectedResults);

  UrlbarProvidersManager.unregisterProvider(provider);
  gURLBar.view.close();
});

function checkResults(actual, expected) {
  Assert.equal(actual.length, expected.length, "Number of results");
  for (let i = 0; i < expected.length; i++) {
    info(`Checking results at index ${i}`);
    let actualResult = collectExpectedProperties(actual[i], expected[i]);
    Assert.deepEqual(actualResult, expected[i], "Actual vs. expected result");
  }
}

function collectExpectedProperties(actualObj, expectedObj) {
  let newActualObj = {};
  for (let name in expectedObj) {
    if (typeof expectedObj[name] == "object") {
      newActualObj[name] = collectExpectedProperties(
        actualObj[name],
        expectedObj[name]
      );
    } else {
      newActualObj[name] = expectedObj[name];
    }
  }
  return newActualObj;
}
