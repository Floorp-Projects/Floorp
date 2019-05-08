/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests test the UrlbarController in association with the model.
 */

"use strict";

const {PromiseUtils} = ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");

const TEST_URL = "http://example.com";
const match = new UrlbarResult(UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
                               UrlbarUtils.RESULT_SOURCE.TABS,
                               { url: TEST_URL });
let controller;

/**
 * Asserts that the query context has the expected values.
 *
 * @param {UrlbarQueryContext} context
 * @param {object} expectedValues The expected values for the UrlbarQueryContext.
 */
function assertContextMatches(context, expectedValues) {
  Assert.ok(context instanceof UrlbarQueryContext,
    "Should be a UrlbarQueryContext");

  for (let [key, value] of Object.entries(expectedValues)) {
    Assert.equal(context[key], value,
      `Should have the expected value for ${key} in the UrlbarQueryContext`);
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
  let providerName = registerBasicTestProvider([match]);
  const context = createContext(TEST_URL, {providers: [providerName]});

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
  let providerCanceledDeferred = PromiseUtils.defer();
  let providerName = registerBasicTestProvider([match], providerCanceledDeferred.resolve);
  const context = createContext(TEST_URL, {providers: [providerName]});

  let startedPromise = promiseControllerNotification(controller, "onQueryStarted");
  let cancelPromise = promiseControllerNotification(controller, "onQueryCancelled");

  controller.startQuery(context);

  let params = await startedPromise;

  controller.cancelQuery(context);

  Assert.equal(params[0], context);

  info("Should tell the provider the query is canceled");
  await providerCanceledDeferred.promise;

  params = await cancelPromise;
});
