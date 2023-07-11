/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that a slow heuristic provider returns results normally if it reacts
 * before the longer heuristic chunk timer elapses.
 */

add_task(async function test_chunking_delays() {
  let { UrlbarEventBufferer } = ChromeUtils.importESModule(
    "resource:///modules/UrlbarEventBufferer.sys.mjs"
  );
  Assert.greater(
    UrlbarEventBufferer.DEFERRING_TIMEOUT_MS,
    UrlbarProvidersManager.CHUNK_HEURISTIC_RESULTS_DELAY_MS,
    "The deferring timeout must be larger than the heuristic chunk timer"
  );
});

add_task(async function test_normal() {
  // In a normal situation, we fire the chunking timer early, as soon as all the
  // heuristic providers reported back.
  let origTimer = UrlbarProvidersManager.CHUNK_HEURISTIC_RESULTS_DELAY_MS;
  // Set to a large value, results should be returned earlier.
  let longTimeoutMs = 10000;
  UrlbarProvidersManager.CHUNK_HEURISTIC_RESULTS_DELAY_MS = longTimeoutMs;

  // Add a heuristic provider returning no results, and check we don't wait
  // for the whole heuristic timeout, because of that.
  let noResultsHeuristicProvider = new UrlbarTestUtils.TestProvider({
    results: [], // Empty results provider
    name: "TestHeuristic",
    type: UrlbarUtils.PROVIDER_TYPE.HEURISTIC,
  });
  UrlbarProvidersManager.registerProvider(noResultsHeuristicProvider);

  let fastProfileProvider = new UrlbarTestUtils.TestProvider({
    results: [
      makeVisitResult(createContext("test"), {
        uri: "https://www.example.com/test2",
        title: "test visit for https://www.example.com/test2",
      }),
    ],
    name: "TestNormal",
    type: UrlbarUtils.PROVIDER_TYPE.PROFILE,
  });
  UrlbarProvidersManager.registerProvider(fastProfileProvider);

  let slowProfileProvider = new UrlbarTestUtils.TestProvider({
    results: [],
    name: "TestNormal",
    type: UrlbarUtils.PROVIDER_TYPE.PROFILE,
    addTimeout: longTimeoutMs,
  });
  UrlbarProvidersManager.registerProvider(slowProfileProvider);

  function cleanup() {
    UrlbarProvidersManager.unregisterProvider(noResultsHeuristicProvider);
    UrlbarProvidersManager.unregisterProvider(fastProfileProvider);
    UrlbarProvidersManager.unregisterProvider(slowProfileProvider);
    UrlbarProvidersManager.CHUNK_HEURISTIC_RESULTS_DELAY_MS = origTimer;
  }
  registerCleanupFunction(cleanup);

  let controller = UrlbarTestUtils.newMockController();
  let promiseResultsNotified = new Promise(resolve => {
    let listener = {
      onQueryResults(queryContext) {
        controller.removeQueryListener(listener);
        resolve(queryContext.results);
      },
    };
    controller.addQueryListener(listener);
  });

  let context = createContext("test", {
    providers: [
      noResultsHeuristicProvider.name,
      fastProfileProvider.name,
      slowProfileProvider.name,
    ],
  });
  // Start the timer and don't wait for the query, we want to measure time to
  // the onQueryResults call.
  let startTime = Cu.now();
  let promiseQueryComplete = controller.startQuery(context);
  await promiseResultsNotified;
  let timeDiff = Cu.now() - startTime;
  Assert.greater(
    longTimeoutMs,
    timeDiff,
    `It took ${timeDiff}ms from start to return results`
  );
  // Cancelling the query should return earlier, and not wait for the slow
  // provider.
  controller.cancelQuery();
  await promiseQueryComplete;
  timeDiff = Cu.now() - startTime;
  Assert.greater(
    longTimeoutMs,
    timeDiff,
    `It took ${timeDiff}ms from start to cancel the query`
  );

  cleanup();
});

add_task(async function test_slow() {
  // Must be between CHUNK_RESULTS_DELAY_MS and CHUNK_HEURISTIC_RESULTS_DELAY_MS.
  let timeout = 150;
  Assert.greater(timeout, UrlbarProvidersManager.CHUNK_OTHER_RESULTS_DELAY_MS);
  Assert.greater(
    UrlbarProvidersManager.CHUNK_HEURISTIC_RESULTS_DELAY_MS,
    timeout
  );
  let provider1 = new UrlbarTestUtils.TestProvider({
    results: [
      makeVisitResult(createContext("test"), {
        uri: "https://www.example.com/test",
        title: "test visit for https://www.example.com/test",
        heuristic: true,
      }),
    ],
    name: "TestHeuristic",
    type: UrlbarUtils.PROVIDER_TYPE.HEURISTIC,
    addTimeout: timeout,
  });
  UrlbarProvidersManager.registerProvider(provider1);
  let provider2 = new UrlbarTestUtils.TestProvider({
    results: [
      makeVisitResult(createContext("test"), {
        uri: "https://www.example.com/test2",
        title: "test visit for https://www.example.com/test2",
      }),
    ],
    name: "TestNormal",
    type: UrlbarUtils.PROVIDER_TYPE.PROFILE,
  });
  UrlbarProvidersManager.registerProvider(provider2);
  registerCleanupFunction(() => {
    UrlbarProvidersManager.unregisterProvider(provider1);
    UrlbarProvidersManager.unregisterProvider(provider2);
  });
  let controller = UrlbarTestUtils.newMockController();
  let promiseResultsNotified = new Promise(resolve => {
    let listener = {
      onQueryResults(queryContext) {
        controller.removeQueryListener(listener);
        resolve(queryContext.results);
      },
    };
    controller.addQueryListener(listener);
  });
  let context = createContext("test", {
    providers: [provider1.name, provider2.name],
  });
  await controller.startQuery(context);
  let results = await promiseResultsNotified;
  info(
    "Results:\n" + results.map(m => `${m.title} - ${m.payload.url}`).join("\n")
  );
  Assert.equal(results.length, 2, "Found the expected number of results");
  Assert.ok(context.results[0].heuristic);
  Assert.equal(context.results[0].payload.url, "https://www.example.com/test");
});
