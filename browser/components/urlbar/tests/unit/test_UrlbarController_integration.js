/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests test the UrlbarController in association with the model.
 */

"use strict";

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
  let providerCanceledDeferred = Promise.withResolvers();
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

  let delayResultsPromise = new Promise(resolve => {
    controller.addQueryListener({
      async onQueryResults(queryContext) {
        controller.removeQueryListener(this);
        controller.cancelQuery(queryContext);
        resolve();
      },
    });
  });

  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    { url: "https://example.com/1", title: "example" }
  );

  // We are awaiting for asynchronous work on initialization.
  // For this test, we need the query objects to be created. We ensure this by
  // using a delayed Provider. We wait for onQueryResults, then cancel the
  // query. By that time the query objects are created. Then we unblock the
  // delayed provider.
  let delayedProvider = new UrlbarTestUtils.TestProvider({
    delayResultsPromise,
    results: [result],
    type: UrlbarUtils.PROVIDER_TYPE.PROFILE,
  });

  UrlbarProvidersManager.registerProvider(delayedProvider);

  controller.startQuery(context);

  let params = await startedPromise;
  Assert.equal(params[0], context);

  info("Should have notified the provider the query is canceled");
  await providerCanceledDeferred.promise;

  params = await cancelPromise;
  UrlbarProvidersManager.unregisterProvider(delayedProvider);
});
