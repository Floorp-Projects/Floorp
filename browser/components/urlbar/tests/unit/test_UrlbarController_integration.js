/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests test the UrlbarController in association with the model.
 */

"use strict";

ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");

const TEST_URL = "http://example.com";
const match = new UrlbarMatch(UrlbarUtils.MATCH_TYPE.TAB_SWITCH, { url: TEST_URL });
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
  controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
});

add_task(async function test_basic_search() {
  const context = createContext(TEST_URL);

  registerBasicTestProvider([match]);

  let startedPromise = promiseControllerNotification(controller, "onQueryStarted");
  let resultsPromise = promiseControllerNotification(controller, "onQueryResults");

  controller.startQuery(context);

  let params = await startedPromise;

  Assert.equal(params[0], context);

  params = await resultsPromise;

  Assert.deepEqual(params[0].results, [match],
    "Should have the expected match");
});

add_task(async function test_cancel_search() {
  const context = createContext(TEST_URL);

  let providerCanceledPromise = PromiseUtils.defer();
  registerBasicTestProvider([match], providerCanceledPromise.resolve);

  let startedPromise = promiseControllerNotification(controller, "onQueryStarted");
  let cancelPromise = promiseControllerNotification(controller, "onQueryResults");

  await controller.startQuery(context);

  let params = await startedPromise;

  controller.cancelQuery(context);

  Assert.equal(params[0], context);

  info("Should tell the provider the query is canceled");
  await providerCanceledPromise;

  params = await cancelPromise;
});
