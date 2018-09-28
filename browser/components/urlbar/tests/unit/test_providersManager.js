/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_providers() {
  // First unregister all the existing providers.
  for (let providers of UrlbarProvidersManager.providers.values()) {
    for (let provider of providers.values()) {
      // While here check all providers have name and type.
      Assert.ok(Object.values(UrlbarUtils.PROVIDER_TYPE).includes(provider.type),
        `The provider "${provider.name}" should have a valid type`);
      Assert.ok(provider.name, "All providers should have a name");
      UrlbarProvidersManager.unregisterProvider(provider);
    }
  }
  let match = new UrlbarMatch(UrlbarUtils.MATCH_TYPE.TAB_SWITCH, { url: "http://mozilla.org/foo/" });
  UrlbarProvidersManager.registerProvider({
    get name() {
      return "TestProvider";
    },
    get type() {
      return UrlbarUtils.PROVIDER_TYPE.PROFILE;
    },
    async startQuery(context, add) {
      Assert.ok(context, "context is passed-in");
      Assert.equal(typeof add, "function", "add is a callback");
      this._context = context;
      add(this, match);
    },
    cancelQuery(context) {
      Assert.equal(this._context, context, "context is the same");
    },
  });

  let context = createContext();
  let controller = new UrlbarController();
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
