/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  // These two lines are necessary because UrlbarMuxerUnifiedComplete.sort calls
  // PlacesSearchAutocompleteProvider.parseSubmissionURL, so we need engines and
  // PlacesSearchAutocompleteProvider.
  await AddonTestUtils.promiseStartupManager();
  await PlacesSearchAutocompleteProvider.ensureReady();
});

add_task(async function test_filtering_disable_only_source() {
  let match = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
    UrlbarUtils.RESULT_SOURCE.TABS,
    { url: "http://mozilla.org/foo/" }
  );
  let providerName = registerBasicTestProvider([match]);
  let context = createContext(undefined, { providers: [providerName] });
  let controller = UrlbarTestUtils.newMockController();

  info("Disable the only available source, should get no matches");
  Services.prefs.setBoolPref("browser.urlbar.suggest.openpage", false);
  let promise = Promise.race([
    promiseControllerNotification(controller, "onQueryResults", false),
    promiseControllerNotification(controller, "onQueryFinished"),
  ]);
  await controller.startQuery(context);
  await promise;
  Services.prefs.clearUserPref("browser.urlbar.suggest.openpage");
  UrlbarProvidersManager.unregisterProvider({ name: providerName });
});

add_task(async function test_filtering_disable_one_source() {
  let matches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      UrlbarUtils.RESULT_SOURCE.TABS,
      { url: "http://mozilla.org/foo/" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/foo/" }
    ),
  ];
  let providerName = registerBasicTestProvider(matches);
  let context = createContext(undefined, { providers: [providerName] });
  let controller = UrlbarTestUtils.newMockController();

  info("Disable one of the sources, should get a single match");
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  let promise = Promise.all([
    promiseControllerNotification(controller, "onQueryResults"),
    promiseControllerNotification(controller, "onQueryFinished"),
  ]);
  await controller.startQuery(context, controller);
  await promise;
  Assert.deepEqual(context.results, matches.slice(0, 1));
  Services.prefs.clearUserPref("browser.urlbar.suggest.history");
  UrlbarProvidersManager.unregisterProvider({ name: providerName });
});

add_task(async function test_filtering_restriction_token() {
  let matches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      UrlbarUtils.RESULT_SOURCE.TABS,
      { url: "http://mozilla.org/foo/" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/foo/" }
    ),
  ];
  let providerName = registerBasicTestProvider(matches);
  let context = createContext(`foo ${UrlbarTokenizer.RESTRICT.OPENPAGE}`, {
    providers: [providerName],
  });
  let controller = UrlbarTestUtils.newMockController();

  info("Use a restriction character, should get a single match");
  let promise = Promise.all([
    promiseControllerNotification(controller, "onQueryResults"),
    promiseControllerNotification(controller, "onQueryFinished"),
  ]);
  await controller.startQuery(context, controller);
  await promise;
  Assert.deepEqual(context.results, matches.slice(0, 1));
  UrlbarProvidersManager.unregisterProvider({ name: providerName });
});

add_task(async function test_filter_javascript() {
  let match = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
    UrlbarUtils.RESULT_SOURCE.TABS,
    { url: "http://mozilla.org/foo/" }
  );
  let jsMatch = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    { url: "javascript:foo" }
  );
  let providerName = registerBasicTestProvider([match, jsMatch]);
  let context = createContext(undefined, { providers: [providerName] });
  let controller = UrlbarTestUtils.newMockController();

  info("By default javascript should be filtered out");
  let promise = promiseControllerNotification(controller, "onQueryResults");
  await controller.startQuery(context, controller);
  await promise;
  Assert.deepEqual(context.results, [match]);

  info("Except when the user explicitly starts the search with javascript:");
  context = createContext(`javascript: ${UrlbarTokenizer.RESTRICT.HISTORY}`, {
    providers: [providerName],
  });
  promise = promiseControllerNotification(controller, "onQueryResults");
  await controller.startQuery(context, controller);
  await promise;
  Assert.deepEqual(context.results, [jsMatch]);

  info("Disable javascript filtering");
  Services.prefs.setBoolPref("browser.urlbar.filter.javascript", false);
  context = createContext(undefined, { providers: [providerName] });
  promise = promiseControllerNotification(controller, "onQueryResults");
  await controller.startQuery(context, controller);
  await promise;
  Assert.deepEqual(context.results, [match, jsMatch]);
  Services.prefs.clearUserPref("browser.urlbar.filter.javascript");
  UrlbarProvidersManager.unregisterProvider({ name: providerName });
});

add_task(async function test_filter_isActive() {
  let goodMatches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      UrlbarUtils.RESULT_SOURCE.TABS,
      { url: "http://mozilla.org/foo/" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/foo/" }
    ),
  ];
  let providerName = registerBasicTestProvider(goodMatches);

  let badMatches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      { url: "http://mozilla.org/foo/" }
    ),
  ];
  /**
   * A test provider that should not be invoked.
   */
  class NoInvokeProvider extends UrlbarProvider {
    get name() {
      return "BadProvider";
    }
    get type() {
      return UrlbarUtils.PROVIDER_TYPE.PROFILE;
    }
    isActive(context) {
      info("Acceptable sources: " + context.sources);
      return context.sources.includes(UrlbarUtils.RESULT_SOURCE.BOOKMARKS);
    }
    async startQuery(context, add) {
      Assert.ok(false, "Provider should no be invoked");
      for (const match of badMatches) {
        add(this, match);
      }
    }
    cancelQuery(context) {}
    pickResult(result) {}
  }
  UrlbarProvidersManager.registerProvider(new NoInvokeProvider());

  let context = createContext(undefined, {
    sources: [UrlbarUtils.RESULT_SOURCE.TABS],
    providers: [providerName, "BadProvider"],
  });
  let controller = UrlbarTestUtils.newMockController();

  info("Only tabs should be returned");
  let promise = promiseControllerNotification(controller, "onQueryResults");
  await controller.startQuery(context, controller);
  await promise;
  Assert.deepEqual(context.results.length, 1, "Should find only one match");
  Assert.deepEqual(
    context.results[0].source,
    UrlbarUtils.RESULT_SOURCE.TABS,
    "Should find only a tab match"
  );
  UrlbarProvidersManager.unregisterProvider({ name: providerName });
  UrlbarProvidersManager.unregisterProvider({ name: "BadProvider" });
});

add_task(async function test_filter_queryContext() {
  let providerName = registerBasicTestProvider();

  /**
   * A test provider that should not be invoked because of queryContext.providers.
   */
  class NoInvokeProvider extends UrlbarProvider {
    get name() {
      return "BadProvider";
    }
    get type() {
      return UrlbarUtils.PROVIDER_TYPE.PROFILE;
    }
    isActive(context) {
      return true;
    }
    async startQuery(context, add) {
      Assert.ok(false, "Provider should no be invoked");
    }
    cancelQuery(context) {}
    pickResult(result) {}
  }
  UrlbarProvidersManager.registerProvider(new NoInvokeProvider());

  let context = createContext(undefined, {
    providers: [providerName],
  });
  let controller = UrlbarTestUtils.newMockController();

  await controller.startQuery(context, controller);
  UrlbarProvidersManager.unregisterProvider({ name: providerName });
  UrlbarProvidersManager.unregisterProvider({ name: "BadProvider" });
});

add_task(async function test_nofilter_heuristic() {
  // Checks that even if a provider returns a result that should be filtered out
  // it will still be invoked if it's of type heuristic, and only the heuristic
  // result is returned.
  let matches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      UrlbarUtils.RESULT_SOURCE.TABS,
      { url: "http://mozilla.org/foo/" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      UrlbarUtils.RESULT_SOURCE.TABS,
      { url: "http://mozilla.org/foo2/" }
    ),
  ];
  matches[0].heuristic = true;
  let providerName = registerBasicTestProvider(
    matches,
    undefined,
    UrlbarUtils.PROVIDER_TYPE.HEURISTIC
  );

  let context = createContext(undefined, {
    sources: [UrlbarUtils.RESULT_SOURCE.SEARCH],
    providers: [providerName],
  });
  let controller = UrlbarTestUtils.newMockController();

  // Disable search matches through prefs.
  Services.prefs.setBoolPref("browser.urlbar.suggest.openpage", false);
  info("Only 1 heuristic tab result should be returned");
  let promise = promiseControllerNotification(controller, "onQueryResults");
  await controller.startQuery(context, controller);
  await promise;
  Services.prefs.clearUserPref("browser.urlbar.suggest.openpage");
  Assert.deepEqual(context.results.length, 1, "Should find only one match");
  Assert.deepEqual(
    context.results[0].source,
    UrlbarUtils.RESULT_SOURCE.TABS,
    "Should find only a tab match"
  );
  UrlbarProvidersManager.unregisterProvider({ name: providerName });
});

add_task(async function test_nofilter_restrict() {
  // Checks that even if a pref is disabled, we still return results on a
  // restriction token.
  let matches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      UrlbarUtils.RESULT_SOURCE.TABS,
      { url: "http://mozilla.org/foo_tab/" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      { url: "http://mozilla.org/foo_bookmark/" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/foo_history/" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      { engine: "noengine" }
    ),
  ];
  /**
   * A test provider.
   */
  class TestProvider extends UrlbarProvider {
    get name() {
      return "MyProvider";
    }
    get type() {
      return UrlbarUtils.PROVIDER_TYPE.PROFILE;
    }
    isActive(context) {
      Assert.equal(context.sources.length, 1, "Check acceptable sources");
      return true;
    }
    async startQuery(context, add) {
      Assert.ok(true, "expected provider was invoked");
      for (let match of matches) {
        add(this, match);
      }
    }
    cancelQuery(context) {}
    pickResult(result) {}
  }
  let provider = new TestProvider();
  UrlbarProvidersManager.registerProvider(provider);

  let typeToPropertiesMap = new Map([
    ["HISTORY", { source: "HISTORY", pref: "history" }],
    ["BOOKMARK", { source: "BOOKMARKS", pref: "bookmark" }],
    ["OPENPAGE", { source: "TABS", pref: "openpage" }],
    ["SEARCH", { source: "SEARCH", pref: "searches" }],
  ]);
  for (let [type, token] of Object.entries(UrlbarTokenizer.RESTRICT)) {
    let properties = typeToPropertiesMap.get(type);
    if (!properties) {
      continue;
    }
    info("Restricting on " + type);
    let context = createContext(token + " foo", {
      providers: ["MyProvider"],
    });
    let controller = UrlbarTestUtils.newMockController();
    // Disable the corresponding pref.
    const pref = "browser.urlbar.suggest." + properties.pref;
    info("Disabling " + pref);
    Services.prefs.setBoolPref(pref, false);
    await controller.startQuery(context, controller);
    Assert.equal(context.results.length, 1, "Should find one result");
    Assert.equal(
      context.results[0].source,
      UrlbarUtils.RESULT_SOURCE[properties.source],
      "Check result source"
    );
    Services.prefs.clearUserPref(pref);
  }
  UrlbarProvidersManager.unregisterProvider(provider);
});

add_task(async function test_filter_priority() {
  /**
   * A test provider.
   */
  class TestProvider extends UrlbarProvider {
    constructor(priority, shouldBeInvoked, namePart = "") {
      super();
      this._priority = priority;
      this._name = `Provider-${priority}` + namePart;
      this._shouldBeInvoked = shouldBeInvoked;
    }
    get name() {
      return this._name;
    }
    get type() {
      return UrlbarUtils.PROVIDER_TYPE.PROFILE;
    }
    isActive(context) {
      return true;
    }
    getPriority(context) {
      return this._priority;
    }
    async startQuery(context, add) {
      Assert.ok(this._shouldBeInvoked, `${this.name} was invoked`);
    }
    cancelQuery(context) {}
    pickResult(result) {}
  }

  // Test all possible orderings of the providers to make sure the logic that
  // finds the highest priority providers is correct.
  let providerPerms = permute([
    new TestProvider(0, false),
    new TestProvider(1, false),
    new TestProvider(2, true, "a"),
    new TestProvider(2, true, "b"),
  ]);
  for (let providers of providerPerms) {
    for (let provider of providers) {
      UrlbarProvidersManager.registerProvider(provider);
    }
    let providerNames = providers.map(p => p.name);
    let context = createContext(undefined, { providers: providerNames });
    let controller = UrlbarTestUtils.newMockController();
    await controller.startQuery(context, controller);
    for (let name of providerNames) {
      UrlbarProvidersManager.unregisterProvider({ name });
    }
  }
});

function permute(objects) {
  if (objects.length <= 1) {
    return [objects];
  }
  let perms = [];
  for (let i = 0; i < objects.length; i++) {
    let otherObjects = objects.slice();
    otherObjects.splice(i, 1);
    let otherPerms = permute(otherObjects);
    for (let perm of otherPerms) {
      perm.unshift(objects[i]);
    }
    perms = perms.concat(otherPerms);
  }
  return perms;
}
