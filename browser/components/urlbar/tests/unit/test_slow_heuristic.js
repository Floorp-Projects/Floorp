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

add_task(async function test() {
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
