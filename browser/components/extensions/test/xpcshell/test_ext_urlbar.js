"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProviderExtension: "resource:///modules/UrlbarProviderExtension.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarQueryContext: "resource:///modules/UrlbarUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

const ORIGINAL_NOTIFICATION_TIMEOUT =
  UrlbarProviderExtension.notificationTimeout;

add_task(async function startup() {
  Services.prefs.setCharPref("browser.search.region", "US");
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", false);
  Services.prefs.setIntPref("browser.search.addonLoadTimeout", 0);
  await AddonTestUtils.promiseStartupManager();

  // Add a test engine and make it default so that when we do searches below,
  // Firefox doesn't try to include search suggestions from the actual default
  // engine from over the network.
  let engine = await Services.search.addEngineWithDetails("Test engine", {
    template: "http://example.com/?s=%S",
    alias: "@testengine",
  });
  Services.search.defaultEngine = engine;

  // Set the notification timeout to a really high value to avoid intermittent
  // failures due to the mock extensions not responding in time.
  UrlbarProviderExtension.notificationTimeout = 5000;
});

// Extensions must specify the "urlbar" permission to use browser.urlbar.
add_task(async function test_urlbar_without_urlbar_permission() {
  let ext = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    background() {
      browser.test.assertEq(
        browser.urlbar,
        undefined,
        "'urlbar' permission is required"
      );
    },
  });
  await ext.startup();
  await ext.unload();
});

// Extensions must be privileged to use browser.urlbar.
add_task(async function test_urlbar_no_privilege() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    background() {
      browser.test.assertEq(
        browser.urlbar,
        undefined,
        "'urlbar' permission is privileged"
      );
    },
  });
  await ext.startup();
  await ext.unload();
});

// Checks that providers are added and removed properly.
add_task(async function test_registerProvider() {
  // A copy of the default providers.
  let providers = UrlbarProvidersManager.providers.slice();

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      for (let state of ["active", "inactive", "restricting"]) {
        let name = `Test-${state}`;
        browser.urlbar.onBehaviorRequested.addListener(query => {
          browser.test.assertFalse(query.isPrivate, "Context is non private");
          browser.test.assertEq(query.maxResults, 10, "Check maxResults");
          browser.test.assertTrue(
            query.searchString,
            "SearchString is non empty"
          );
          browser.test.assertTrue(
            Array.isArray(query.acceptableSources),
            "acceptableSources is an array"
          );
          return state;
        }, name);
        browser.urlbar.onResultsRequested.addListener(query => [], name);
      }
    },
  });
  await ext.startup();

  Assert.greater(
    UrlbarProvidersManager.providers.length,
    providers.length,
    "Providers have been added"
  );

  // Run a query, this should execute the above listeners and checks, plus it
  // will set the provider's isActive and isRestricting.
  let queryContext = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "*",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(queryContext);

  // Check the providers behavior has been setup properly.
  for (let provider of UrlbarProvidersManager.providers) {
    if (!provider.name.startsWith("Test")) {
      continue;
    }
    let [, state] = provider.name.split("-");
    let isActive = state != "inactive";
    let restricting = state == "restricting";
    Assert.equal(
      isActive,
      provider.isActive(queryContext),
      "Check active callback"
    );
    Assert.equal(
      restricting,
      provider.isRestricting(queryContext),
      "Check restrict callback"
    );
  }

  await ext.unload();

  // Sanity check the providers.
  Assert.deepEqual(
    UrlbarProvidersManager.providers,
    providers,
    "Should return to the default providers"
  );
});

// Adds a single active provider that returns many kinds of results.  This also
// checks that the heuristic result from the built-in UnifiedComplete provider
// is included.
add_task(async function test_onProviderResultsRequested() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      browser.urlbar.onBehaviorRequested.addListener(query => {
        return "active";
      }, "test");
      browser.urlbar.onResultsRequested.addListener(query => {
        browser.test.assertFalse(query.isPrivate);
        browser.test.assertEq(query.maxResults, 10);
        browser.test.assertEq(query.searchString, "test");
        browser.test.assertTrue(Array.isArray(query.acceptableSources));
        return [
          {
            type: "remote_tab",
            source: "tabs",
            payload: {
              title: "Test remote_tab-tabs result",
              url: "http://example.com/remote_tab-tabs",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              suggestion: "Test search-search result",
              engine: "Test engine",
            },
          },
          {
            type: "tab",
            source: "tabs",
            payload: {
              title: "Test tab-tabs result",
              url: "http://example.com/tab-tabs",
            },
          },
          {
            type: "url",
            source: "history",
            payload: {
              title: "Test url-history result",
              url: "http://example.com/url-history",
            },
          },
        ];
      }, "test");
    },
  });
  await ext.startup();

  // Check the provider.
  let provider = UrlbarProvidersManager.getProvider("test");
  Assert.ok(provider);

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(context);

  // Check isActive and isRestricting.
  Assert.ok(provider.isActive(context));
  Assert.ok(!provider.isRestricting(context));

  // Check the results.
  let expectedResults = [
    // The first result should be a search result returned by the
    // UnifiedComplete provider.
    {
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      title: "test",
      heuristic: true,
    },
    // The second result should be our search suggestion result since the
    // default muxer sorts search suggestion results before other types.
    {
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      title: "Test search-search result",
      heuristic: false,
    },
    // The rest of the results should appear in the order we returned them
    // above.
    {
      type: UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
      source: UrlbarUtils.RESULT_SOURCE.TABS,
      title: "Test remote_tab-tabs result",
      heuristic: false,
    },
    {
      type: UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      source: UrlbarUtils.RESULT_SOURCE.TABS,
      title: "Test tab-tabs result",
      heuristic: false,
    },
    {
      type: UrlbarUtils.RESULT_TYPE.URL,
      source: UrlbarUtils.RESULT_SOURCE.HISTORY,
      title: "Test url-history result",
      heuristic: false,
    },
  ];

  let actualResults = context.results.map(r => ({
    type: r.type,
    source: r.source,
    title: r.title,
    heuristic: r.heuristic,
  }));

  Assert.deepEqual(actualResults, expectedResults);

  await ext.unload();
});

// Extensions can specify search engines using engine names, aliases, and URLs.
add_task(async function test_onProviderResultsRequested_searchEngines() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      browser.urlbar.onBehaviorRequested.addListener(query => {
        return "restricting";
      }, "test");
      browser.urlbar.onResultsRequested.addListener(query => {
        return [
          {
            type: "search",
            source: "search",
            payload: {
              engine: "Test engine",
              suggestion: "engine specified",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              keyword: "@testengine",
              suggestion: "keyword specified",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              url: "http://example.com/?s",
              suggestion: "url specified",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              engine: "Test engine",
              keyword: "@testengine",
              url: "http://example.com/?s",
              suggestion: "engine, keyword, and url specified",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              keyword: "@testengine",
              url: "http://example.com/?s",
              suggestion: "keyword and url specified",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              suggestion: "no engine",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              engine: "bogus",
              suggestion: "no matching engine",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              keyword: "@bogus",
              suggestion: "no matching keyword",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              url: "http://bogus-no-search-engine.com/",
              suggestion: "no matching url",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              url: "bogus",
              suggestion: "invalid url",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              url: "foo:bar",
              suggestion: "url with no hostname",
            },
          },
        ];
      }, "test");
    },
  });
  await ext.startup();

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(context);

  // Check the results.  The first several are valid and should include "Test
  // engine" as the engine.  The others don't specify an engine and are
  // therefore invalid, so they should be ignored.
  let expectedResults = [
    {
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      engine: "Test engine",
      title: "engine specified",
      heuristic: false,
    },
    {
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      engine: "Test engine",
      title: "keyword specified",
      heuristic: false,
    },
    {
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      engine: "Test engine",
      title: "url specified",
      heuristic: false,
    },
    {
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      engine: "Test engine",
      title: "engine, keyword, and url specified",
      heuristic: false,
    },
    {
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      engine: "Test engine",
      title: "keyword and url specified",
      heuristic: false,
    },
  ];

  let actualResults = context.results.map(r => ({
    type: r.type,
    source: r.source,
    engine: r.payload.engine || null,
    title: r.title,
    heuristic: r.heuristic,
  }));

  Assert.deepEqual(actualResults, expectedResults);

  await ext.unload();
});

// Adds two providers, one active and one inactive.  Only the active provider
// should be asked to return results.
add_task(async function test_activeAndInactiveProviders() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      for (let behavior of ["active", "inactive"]) {
        browser.urlbar.onBehaviorRequested.addListener(query => {
          return behavior;
        }, behavior);
        browser.urlbar.onResultsRequested.addListener(query => {
          browser.test.assertEq(
            behavior,
            "active",
            "onResultsRequested should be fired only for the active provider"
          );
          return [
            {
              type: "url",
              source: "history",
              payload: {
                title: `Test result ${behavior}`,
                url: `http://example.com/${behavior}`,
              },
            },
          ];
        }, behavior);
      }
    },
  });
  await ext.startup();

  // Check the providers.
  let active = UrlbarProvidersManager.getProvider("active");
  let inactive = UrlbarProvidersManager.getProvider("inactive");
  Assert.ok(active);
  Assert.ok(inactive);

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(context);

  // Check isActive and isRestricting.
  Assert.ok(active.isActive(context));
  Assert.ok(!inactive.isActive(context));
  Assert.ok(!active.isRestricting(context));
  Assert.ok(!inactive.isRestricting(context));

  // Check the results.
  Assert.equal(context.results.length, 2);
  Assert.ok(context.results[0].heuristic);
  Assert.equal(context.results[1].title, "Test result active");

  await ext.unload();
});

// Adds three active providers.  They all should be asked for results.
add_task(async function test_threeActiveProviders() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      for (let i = 0; i < 3; i++) {
        let name = `test-${i}`;
        browser.urlbar.onBehaviorRequested.addListener(query => {
          return "active";
        }, name);
        browser.urlbar.onResultsRequested.addListener(query => {
          return [
            {
              type: "url",
              source: "history",
              payload: {
                title: `Test result ${i}`,
                url: `http://example.com/${i}`,
              },
            },
          ];
        }, name);
      }
    },
  });
  await ext.startup();

  // Check the providers.
  let providers = [];
  for (let i = 0; i < 3; i++) {
    let name = `test-${i}`;
    let provider = UrlbarProvidersManager.getProvider(name);
    Assert.ok(provider);
    providers.push(provider);
  }

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(context);

  // Check isActive and isRestricting.
  for (let provider of providers) {
    Assert.ok(provider.isActive(context));
    Assert.ok(!provider.isRestricting(context));
  }

  // Check the results.
  Assert.equal(context.results.length, 4);
  Assert.ok(context.results[0].heuristic);
  for (let i = 0; i < providers.length; i++) {
    Assert.equal(context.results[i + 1].title, `Test result ${i}`);
  }

  await ext.unload();
});

// Adds three inactive providers.  None of them should be asked for results.
// This also checks that provider behavior is "inactive" by default.
add_task(async function test_threeInactiveProviders() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      for (let i = 0; i < 3; i++) {
        // Don't add an onBehaviorRequested listener.  That way we can test that
        // the default behavior is inactive.
        browser.urlbar.onResultsRequested.addListener(query => {
          browser.test.notifyFail(
            "onResultsRequested fired for inactive provider"
          );
        }, `test-${i}`);
      }
    },
  });
  await ext.startup();

  // Check the providers.
  let providers = [];
  for (let i = 0; i < 3; i++) {
    let name = `test-${i}`;
    let provider = UrlbarProvidersManager.getProvider(name);
    Assert.ok(provider);
    providers.push(provider);
  }

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(context);

  // Check isActive and isRestricting.
  for (let provider of providers) {
    Assert.ok(!provider.isActive(context));
    Assert.ok(!provider.isRestricting(context));
  }

  // Check the results.
  Assert.equal(context.results.length, 1);
  Assert.ok(context.results[0].heuristic);

  await ext.unload();
});

// Adds active, inactive, and restricting providers.  Only the restricting
// provider should be asked to return results.
add_task(async function test_activeInactiveAndRestrictingProviders() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      for (let behavior of ["active", "inactive", "restricting"]) {
        browser.urlbar.onBehaviorRequested.addListener(query => {
          return behavior;
        }, behavior);
        browser.urlbar.onResultsRequested.addListener(query => {
          browser.test.assertEq(
            behavior,
            "restricting",
            "onResultsRequested should be fired for the restricting provider"
          );
          return [
            {
              type: "url",
              source: "history",
              payload: {
                title: `Test result ${behavior}`,
                url: `http://example.com/${behavior}`,
              },
            },
          ];
        }, behavior);
      }
    },
  });
  await ext.startup();

  // Check the providers.
  let providers = {};
  for (let behavior of ["active", "inactive", "restricting"]) {
    let provider = UrlbarProvidersManager.getProvider(behavior);
    Assert.ok(provider);
    providers[behavior] = provider;
  }

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(context);

  // Check isActive and isRestricting.
  Assert.ok(providers.active.isActive(context));
  Assert.ok(!providers.active.isRestricting(context));
  Assert.ok(!providers.inactive.isActive(context));
  Assert.ok(!providers.inactive.isRestricting(context));
  Assert.ok(providers.restricting.isActive(context));
  Assert.ok(providers.restricting.isRestricting(context));

  // Check the results.
  Assert.equal(context.results.length, 1);
  Assert.equal(context.results[0].title, "Test result restricting");

  await ext.unload();
});

// Adds an active provider that doesn't have a listener for onResultsRequested.
// No results should be added.
add_task(async function test_onResultsRequestedNotImplemented() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      browser.urlbar.onBehaviorRequested.addListener(query => {
        return "active";
      }, "test");
    },
  });
  await ext.startup();

  // Check the provider.
  let provider = UrlbarProvidersManager.getProvider("test");
  Assert.ok(provider);

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(context);

  // Check isActive and isRestricting.
  Assert.ok(provider.isActive(context));
  Assert.ok(!provider.isRestricting(context));

  // Check the results.
  Assert.equal(context.results.length, 1);
  Assert.ok(context.results[0].heuristic);

  await ext.unload();
});

// Adds an active provider that returns a result with a malformed payload.  The
// bad result shouldn't be added.
add_task(async function test_badPayload() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      browser.urlbar.onBehaviorRequested.addListener(query => {
        return "active";
      }, "test");
      browser.urlbar.onResultsRequested.addListener(async query => {
        return [
          {
            type: "url",
            source: "history",
            payload: "this is a bad payload",
          },
          {
            type: "url",
            source: "history",
            payload: {
              title: "Test result",
              url: "http://example.com/",
            },
          },
        ];
      }, "test");
    },
  });
  await ext.startup();

  // Check the provider.
  let provider = UrlbarProvidersManager.getProvider("test");
  Assert.ok(provider);

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(context);

  // Check the results.
  Assert.equal(context.results.length, 2);
  Assert.ok(context.results[0].heuristic);
  Assert.equal(context.results[1].title, "Test result");

  await ext.unload();
});

// Tests the onQueryCanceled event.
add_task(async function test_onQueryCanceled() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      browser.urlbar.onBehaviorRequested.addListener(query => {
        return "active";
      }, "test");
      browser.urlbar.onQueryCanceled.addListener(query => {
        browser.test.notifyPass("canceled");
      }, "test");
    },
  });
  await ext.startup();

  // Check the provider.
  let provider = UrlbarProvidersManager.getProvider("test");
  Assert.ok(provider);

  // Run a query but immediately cancel it.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });

  let startPromise = controller.startQuery(context);
  controller.cancelQuery();
  await startPromise;

  await ext.awaitFinish("canceled");

  await ext.unload();
});

// Adds an onBehaviorRequested listener that takes too long to respond.  The
// provider should default to inactive.
add_task(async function test_onBehaviorRequestedTimeout() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      browser.urlbar.onBehaviorRequested.addListener(async query => {
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        await new Promise(r => setTimeout(r, 500));
        return "active";
      }, "test");
      browser.urlbar.onResultsRequested.addListener(query => {
        browser.test.notifyFail(
          "onResultsRequested fired for inactive provider"
        );
      }, "test");
    },
  });
  await ext.startup();

  // Check the provider.
  let provider = UrlbarProvidersManager.getProvider("test");
  Assert.ok(provider);

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });

  let currentTimeout = UrlbarProviderExtension.notificationTimeout;
  UrlbarProviderExtension.notificationTimeout = 0;
  await controller.startQuery(context);
  UrlbarProviderExtension.notificationTimeout = currentTimeout;

  // Check isActive and isRestricting.
  Assert.ok(!provider.isActive(context));
  Assert.ok(!provider.isRestricting(context));

  // Check the results.
  Assert.equal(context.results.length, 1);
  Assert.ok(context.results[0].heuristic);

  await ext.unload();
});

// Adds an onResultsRequested listener that takes too long to respond.  The
// provider's results should default to no results.
add_task(async function test_onResultsRequestedTimeout() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      browser.urlbar.onBehaviorRequested.addListener(query => {
        return "active";
      }, "test");
      browser.urlbar.onResultsRequested.addListener(async query => {
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        await new Promise(r => setTimeout(r, 600));
        return [
          {
            type: "url",
            source: "history",
            payload: {
              title: "Test result",
              url: "http://example.com/",
            },
          },
        ];
      }, "test");
    },
  });
  await ext.startup();

  // Check the provider.
  let provider = UrlbarProvidersManager.getProvider("test");
  Assert.ok(provider);

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });

  // Set the notification timeout.  In test_onBehaviorRequestedTimeout above, we
  // could set it to 0 because we were testing onBehaviorRequested, which is
  // fired first.  Here we're testing onResultsRequested, which is fired after
  // onBehaviorRequested.  So we must first respond to onBehaviorRequested but
  // then time out on onResultsRequested, and that's why the timeout can't be 0.
  let currentTimeout = UrlbarProviderExtension.notificationTimeout;
  UrlbarProviderExtension.notificationTimeout = ORIGINAL_NOTIFICATION_TIMEOUT;
  await controller.startQuery(context);
  UrlbarProviderExtension.notificationTimeout = currentTimeout;

  // Check isActive and isRestricting.
  Assert.ok(provider.isActive(context));
  Assert.ok(!provider.isRestricting(context));

  // Check the results.
  Assert.equal(context.results.length, 1);
  Assert.ok(context.results[0].heuristic);

  await ext.unload();
});

// Performs a search in a private context for an extension that does not allow
// private browsing.  The extension's listeners should not be called.
add_task(async function test_privateBrowsing_not_allowed() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
      incognito: "not_allowed",
    },
    isPrivileged: true,
    background() {
      browser.urlbar.onBehaviorRequested.addListener(query => {
        browser.test.notifyFail(
          "onBehaviorRequested fired in private browsing"
        );
      }, "Test-private");
      browser.urlbar.onResultsRequested.addListener(query => {
        browser.test.notifyFail("onResultsRequested fired in private browsing");
      }, "Test-private");
      // We can't easily test onQueryCanceled here because immediately canceling
      // the query will cause onResultsRequested not to be fired.
      // onResultsRequested should in fact not be fired, but that should be
      // because this test runs in private-browsing mode, not because the query
      // was canceled.  See the next test task for onQueryCanceled.
    },
  });
  await ext.startup();

  // Run a query, this should execute the above listeners and checks, plus it
  // will set the provider's isActive and isRestricting.
  let queryContext = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: true,
    maxResults: 10,
    searchString: "*",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(queryContext);
  // Check the providers behavior has been setup properly.
  let provider = UrlbarProvidersManager.getProvider("Test-private");
  Assert.ok(!provider.isActive({}), "Check provider is inactive");

  await ext.unload();
});

// Same as the previous task but tests the onQueryCanceled event: Performs a
// search in a private context for an extension that does not allow private
// browsing.  The extension's onQueryCanceled listener should not be called.
add_task(async function test_privateBrowsing_not_allowed_onQueryCanceled() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
      incognito: "not_allowed",
    },
    isPrivileged: true,
    background() {
      browser.urlbar.onBehaviorRequested.addListener(query => {
        browser.test.notifyFail(
          "onBehaviorRequested fired in private browsing"
        );
      }, "test");
      browser.urlbar.onQueryCanceled.addListener(query => {
        browser.test.notifyFail("onQueryCanceled fired in private browsing");
      }, "test");
    },
  });
  await ext.startup();

  // Check the provider.
  let provider = UrlbarProvidersManager.getProvider("test");
  Assert.ok(provider);

  // Run a query but immediately cancel it.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: true,
    maxResults: 10,
    searchString: "*",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });

  let startPromise = controller.startQuery(context);
  controller.cancelQuery();
  await startPromise;

  // Check isActive and isRestricting.
  Assert.ok(!provider.isActive(context));
  Assert.ok(!provider.isRestricting(context));

  await ext.unload();
});

// Performs a search in a private context for an extension that allows private
// browsing.  The extension's listeners should be called.
add_task(async function test_privateBrowsing_allowed() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      let name = "Test-private";
      browser.urlbar.onBehaviorRequested.addListener(query => {
        browser.test.sendMessage("onBehaviorRequested");
        return "active";
      }, name);
      browser.urlbar.onResultsRequested.addListener(query => {
        browser.test.sendMessage("onResultsRequested");
        return [];
      }, name);
      // We can't easily test onQueryCanceled here because immediately canceling
      // the query will cause onResultsRequested not to be fired.  See the next
      // test task for onQueryCanceled.
    },
  });
  await ext.startup();

  // Check the provider.
  let provider = UrlbarProvidersManager.getProvider("Test-private");
  Assert.ok(provider);

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: true,
    maxResults: 10,
    searchString: "*",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(context);

  // Check isActive and isRestricting.
  Assert.ok(provider.isActive(context));
  Assert.ok(!provider.isRestricting(context));

  // The events should have been fired.
  await Promise.all(
    ["onBehaviorRequested", "onResultsRequested"].map(msg =>
      ext.awaitMessage(msg)
    )
  );

  await ext.unload();
});

// Same as the previous task but tests the onQueryCanceled event: Performs a
// search in a private context for an extension that allows private browsing,
// but cancels the search.  The extension's onQueryCanceled listener should be
// called.
add_task(async function test_privateBrowsing_allowed_onQueryCanceled() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      let name = "Test-private";
      browser.urlbar.onBehaviorRequested.addListener(query => {
        browser.test.sendMessage("onBehaviorRequested");
        return "active";
      }, name);
      browser.urlbar.onQueryCanceled.addListener(query => {
        browser.test.sendMessage("onQueryCanceled");
      }, name);
    },
  });
  await ext.startup();

  // Check the provider.
  let provider = UrlbarProvidersManager.getProvider("Test-private");
  Assert.ok(provider);

  // Run a query but immediately cancel it.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: true,
    maxResults: 10,
    searchString: "*",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });

  let startPromise = controller.startQuery(context);
  controller.cancelQuery();
  await startPromise;

  // Check isActive and isRestricting.
  Assert.ok(provider.isActive(context));
  Assert.ok(!provider.isRestricting(context));

  // The events should have been fired.
  await Promise.all(
    ["onBehaviorRequested", "onQueryCanceled"].map(msg => ext.awaitMessage(msg))
  );

  await ext.unload();
});

// Performs a search in a non-private context for an extension that does not
// allow private browsing.  The extension's listeners should be called.
add_task(async function test_nonPrivateBrowsing() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
      incognito: "not_allowed",
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      browser.urlbar.onBehaviorRequested.addListener(query => {
        return "active";
      }, "test");
      browser.urlbar.onResultsRequested.addListener(query => {
        return [
          {
            type: "url",
            source: "history",
            payload: {
              title: "Test result",
              url: "http://example.com/",
            },
          },
        ];
      }, "test");
    },
  });
  await ext.startup();

  // Check the provider.
  let provider = UrlbarProvidersManager.getProvider("test");
  Assert.ok(provider);

  // Run a query.
  let context = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "test",
  });
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  await controller.startQuery(context);

  // Check isActive and isRestricting.
  Assert.ok(provider.isActive(context));
  Assert.ok(!provider.isRestricting(context));

  // Check the results.
  Assert.equal(context.results.length, 2);
  Assert.ok(context.results[0].heuristic);
  Assert.equal(context.results[1].title, "Test result");

  await ext.unload();
});

// Tests the openViewOnFocus property.
add_task(async function test_setOpenViewOnFocus() {
  let getPrefValue = () => UrlbarPrefs.get("openViewOnFocus");

  Assert.equal(
    getPrefValue(),
    false,
    "Open-view-on-focus mode should be disabled by default"
  );

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    useAddonManager: "temporary",
    async background() {
      await browser.urlbar.openViewOnFocus.set({ value: true });
      browser.test.sendMessage("ready");
    },
  });
  await ext.startup();
  await ext.awaitMessage("ready");

  Assert.equal(
    getPrefValue(),
    true,
    "Successfully enabled the open-view-on-focus mode"
  );

  await ext.unload();

  Assert.equal(
    getPrefValue(),
    false,
    "Open-view-on-focus mode should be reset after unloading the add-on"
  );
});

// Tests the engagementTelemetry property.
add_task(async function test_engagementTelemetry() {
  let getPrefValue = () => UrlbarPrefs.get("eventTelemetry.enabled");

  Assert.equal(
    getPrefValue(),
    false,
    "Engagement telemetry should be disabled by default"
  );

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    useAddonManager: "temporary",
    background() {
      browser.urlbar.engagementTelemetry.set({ value: true });
    },
  });
  await ext.startup();

  Assert.equal(
    getPrefValue(),
    true,
    "Successfully enabled the engagement telemetry"
  );

  await ext.unload();

  Assert.equal(
    getPrefValue(),
    false,
    "Engagement telemetry should be reset after unloading the add-on"
  );
});
