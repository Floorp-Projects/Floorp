/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that slow heuristic results are still waited for on selection.

"use strict";

add_task(async function test_slow_heuristic() {
  // Must be between CHUNK_RESULTS_DELAY_MS and DEFERRING_TIMEOUT_MS
  let timeout = 150;
  Assert.greater(timeout, UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS);
  Assert.greater(UrlbarEventBufferer.DEFERRING_TIMEOUT_MS, timeout);

  // First, add a provider that adds a heuristic result on a delay.
  let heuristicResult = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    { url: "https://example.com/" }
  );
  heuristicResult.heuristic = true;
  let heuristicProvider = new UrlbarTestUtils.TestProvider({
    results: [heuristicResult],
    name: "heuristicProvider",
    priority: Infinity,
    addTimeout: timeout,
  });
  UrlbarProvidersManager.registerProvider(heuristicProvider);
  registerCleanupFunction(() => {
    UrlbarProvidersManager.unregisterProvider(heuristicProvider);
  });

  // Do a search without waiting for a result.
  const win = await BrowserTestUtils.openNewBrowserWindow();
  let promiseLoaded = BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser
  );

  win.gURLBar.focus();
  EventUtils.sendString("test", win);
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await promiseLoaded;

  await UrlbarTestUtils.promisePopupClose(win);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_fast_heuristic() {
  let longTimeoutMs = 1000000;
  let originalHeuristicTimeout = UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS;
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = longTimeoutMs;
  registerCleanupFunction(() => {
    UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = originalHeuristicTimeout;
  });

  // Add a fast heuristic provider.
  let heuristicResult = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    { url: "https://example.com/" }
  );
  heuristicResult.heuristic = true;
  let heuristicProvider = new UrlbarTestUtils.TestProvider({
    results: [heuristicResult],
    name: "heuristicProvider",
    priority: Infinity,
  });
  UrlbarProvidersManager.registerProvider(heuristicProvider);
  registerCleanupFunction(() => {
    UrlbarProvidersManager.unregisterProvider(heuristicProvider);
  });

  // Do a search.
  const win = await BrowserTestUtils.openNewBrowserWindow();

  let startTime = Cu.now();
  Assert.greater(
    longTimeoutMs,
    Cu.now() - startTime,
    "Heuristic result is returned faster than CHUNK_RESULTS_DELAY_MS"
  );

  await UrlbarTestUtils.promisePopupClose(win);
  await BrowserTestUtils.closeWindow(win);
});
