/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests test the UrlbarController in association with the model.
 */

"use strict";

const { PromiseUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PromiseUtils.sys.mjs"
);

const TEST_URL = "http://example.com";
const match = new UrlbarResult(
  UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
  UrlbarUtils.RESULT_SOURCE.TABS,
  { url: TEST_URL }
);
let controller;

add_setup(async function () {
  controller = UrlbarTestUtils.newMockController();
});

add_task(async function test_basic_search() {
  let provider = registerBasicTestProvider([match]);
  const context = createContext(TEST_URL, { providers: [provider.name] });

  let startedPromise = promiseControllerNotification(
    controller,
    "onQueryStarted"
  );
  let resultsPromise = promiseControllerNotification(
    controller,
    "onQueryResults"
  );

  controller.startQuery(context);

  let params = await startedPromise;

  Assert.equal(params[0], context);

  params = await resultsPromise;

  Assert.deepEqual(
    params[0].results,
    [match],
    "Should have the expected match"
  );
});

add_task(async function test_cancel_search() {
  let providerCanceledDeferred = PromiseUtils.defer();
  let provider = registerBasicTestProvider(
    [match],
    providerCanceledDeferred.resolve
  );
  const context = createContext(TEST_URL, { providers: [provider.name] });

  let startedPromise = promiseControllerNotification(
    controller,
    "onQueryStarted"
  );
  let cancelPromise = promiseControllerNotification(
    controller,
    "onQueryCancelled"
  );

  controller.startQuery(context);

  let params = await startedPromise;

  controller.cancelQuery(context);

  Assert.equal(params[0], context);

  info("Should tell the provider the query is canceled");
  await providerCanceledDeferred.promise;

  params = await cancelPromise;
});
