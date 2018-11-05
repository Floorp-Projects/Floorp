/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_providers() {
  let match = new UrlbarMatch(UrlbarUtils.MATCH_TYPE.TAB_SWITCH, { url: "http://mozilla.org/foo/" });
  registerBasicTestProvider([match]);

  let context = createContext();
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  let resultsPromise = promiseControllerNotification(controller, "onQueryResults");

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
