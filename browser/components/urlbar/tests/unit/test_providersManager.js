/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_providers() {
  Assert.throws(
    () => UrlbarProvidersManager.registerProvider(),
    /invalid provider/,
    "Should throw with no arguments"
  );
  Assert.throws(
    () => UrlbarProvidersManager.registerProvider({}),
    /invalid provider/,
    "Should throw with empty object"
  );
  Assert.throws(
    () =>
      UrlbarProvidersManager.registerProvider({
        name: "",
      }),
    /invalid provider/,
    "Should throw with empty name"
  );
  Assert.throws(
    () =>
      UrlbarProvidersManager.registerProvider({
        name: "test",
        startQuery: "no",
      }),
    /invalid provider/,
    "Should throw with invalid startQuery"
  );
  Assert.throws(
    () =>
      UrlbarProvidersManager.registerProvider({
        name: "test",
        startQuery: () => {},
        cancelQuery: "no",
      }),
    /invalid provider/,
    "Should throw with invalid cancelQuery"
  );

  let match = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
    UrlbarUtils.RESULT_SOURCE.TABS,
    { url: "http://mozilla.org/foo/" }
  );

  let providerName = registerBasicTestProvider([match]);
  let context = createContext(undefined, { providers: [providerName] });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  let resultsPromise = promiseControllerNotification(
    controller,
    "onQueryResults"
  );

  await UrlbarProvidersManager.startQuery(context, controller);
  // Sanity check that this doesn't throw. It should be a no-op since we await
  // for startQuery.
  UrlbarProvidersManager.cancelQuery(context);

  let params = await resultsPromise;
  Assert.deepEqual(params[0].results, [match]);
});

add_task(async function test_criticalSection() {
  // Just a sanity check, this shouldn't throw.
  await UrlbarProvidersManager.runInCriticalSection(async () => {
    let db = await PlacesUtils.promiseLargeCacheDBConnection();
    await db.execute(`PRAGMA page_cache`);
  });
});
