/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that old results from UrlbarProviderAutofill do not overwrite results
 * from UrlbarProviderHeuristicFallback after the autofillable query is
 * cancelled. See bug 1653436.
 */

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

/**
 * A test provider that waits before returning results to simulate a slow DB
 * lookup.
 */
class SlowHeuristicProvider extends TestProvider {
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
  }

  async startQuery(context, add) {
    this._context = context;
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 300));
    for (let result of this._results) {
      add(this, result);
    }
  }
}

/**
 * A fast provider that alterts the test when it has added its results.
 */
class FastHeuristicProvider extends TestProvider {
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
  }

  async startQuery(context, add) {
    this._context = context;
    for (let result of this._results) {
      add(this, result);
    }
    Services.obs.notifyObservers(null, "results-added");
  }
}

add_task(async function setup() {
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
});

add_task(async function() {
  let context = createContext("m", { isPrivate: false });
  await PlacesTestUtils.promiseAsyncUpdates();
  info("Manually set up query and then overwrite it.");
  // slowProvider is a stand-in for a slow UnifiedComplete returning a
  // non-heuristic result.
  let slowProvider = new SlowHeuristicProvider({
    results: [
      makeVisitResult(context, {
        uri: `http://mozilla.org/`,
        title: `mozilla.org/`,
      }),
    ],
  });
  UrlbarProvidersManager.registerProvider(slowProvider);

  // fastProvider is a stand-in for a fast Autofill returning a heuristic
  // result.
  let fastProvider = new FastHeuristicProvider({
    results: [
      makeVisitResult(context, {
        uri: `http://mozilla.com/`,
        title: `mozilla.com/`,
        heuristic: true,
      }),
    ],
  });
  UrlbarProvidersManager.registerProvider(fastProvider);
  let firstContext = createContext("m", {
    providers: [slowProvider.name, fastProvider.name],
  });
  let secondContext = createContext("ma", {
    providers: [slowProvider.name, fastProvider.name],
  });

  let controller = UrlbarTestUtils.newMockController();
  let queryRecieved, queryCancelled;
  const controllerListener = {
    onQueryResults(queryContext) {
      console.trace(`finished query. context: ${JSON.stringify(queryContext)}`);
      Assert.equal(
        queryContext,
        secondContext,
        "Only the second query should finish."
      );
      queryRecieved = true;
    },
    onQueryCancelled(queryContext) {
      Assert.equal(
        queryContext,
        firstContext,
        "The first query should be cancelled."
      );
      Assert.ok(!queryCancelled, "No more than one query should be cancelled.");
      queryCancelled = true;
    },
  };
  controller.addQueryListener(controllerListener);

  // Wait until FastProvider sends its results to the providers manager.
  // Then they will be queued up in a _heuristicProvidersTimer, waiting for
  // the results from SlowProvider.
  let resultsAddedPromise = new Promise(resolve => {
    let observe = async (subject, topic, data) => {
      Services.obs.removeObserver(observe, "results-added");
      // Fire the second query to cancel the first.
      await controller.startQuery(secondContext);
      resolve();
    };

    Services.obs.addObserver(observe, "results-added");
  });

  controller.startQuery(firstContext);
  await resultsAddedPromise;

  Assert.ok(queryCancelled, "At least one query was cancelled.");
  Assert.ok(queryRecieved, "At least one query finished.");
  controller.removeQueryListener(controllerListener);
});
