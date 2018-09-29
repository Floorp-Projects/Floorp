/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests test the UrlbarController in association with the model.
 */

"use strict";

const TEST_URL = "http://example.com";
let controller;

/**
 * Asserts that the query context has the expected values.
 *
 * @param {QueryContext} context
 * @param {object} expectedValues The expected values for the QueryContext.
 */
function assertContextMatches(context, expectedValues) {
  Assert.ok(context instanceof QueryContext,
    "Should be a QueryContext");

  for (let [key, value] of Object.entries(expectedValues)) {
    Assert.equal(context[key], value,
      `Should have the expected value for ${key} in the QueryContext`);
  }
}

add_task(async function setup() {
  await PlacesTestUtils.addVisits(TEST_URL);

  controller = new UrlbarController();
});

add_task(async function test_basic_search() {
  const context = createContext(TEST_URL);

  let startedPromise = promiseControllerNotification(controller, "onQueryStarted");
  let resultsPromise = promiseControllerNotification(controller, "onQueryResults");

  controller.handleQuery(context);

  let params = await startedPromise;

  Assert.equal(params[0], context);

  params = await resultsPromise;

  Assert.equal(params[0].results.length, Services.prefs.getIntPref("browser.urlbar.maxRichResults"),
    "Should have given the expected amount of results");

  for (let result of params[0].results) {
    Assert.ok(result.url.includes(TEST_URL));
  }
});
