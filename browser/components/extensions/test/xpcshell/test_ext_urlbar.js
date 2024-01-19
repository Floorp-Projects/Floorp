"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  SearchTestUtils: "resource://testing-common/SearchTestUtils.sys.mjs",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.sys.mjs",
  UrlbarQueryContext: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);
SearchTestUtils.init(this);
SearchTestUtils.initXPCShellAddonManager(this, "system");

function getPayload(result) {
  let payload = {};
  for (let [key, value] of Object.entries(result.payload)) {
    if (value !== undefined) {
      payload[key] = value;
    }
  }
  return payload;
}

add_task(async function startup() {
  Services.prefs.setCharPref("browser.search.region", "US");
  Services.prefs.setIntPref("browser.search.addonLoadTimeout", 0);
  Services.prefs.setBoolPref(
    "browser.search.separatePrivateDefault.ui.enabled",
    false
  );
  // Set the notification timeout to a really high value to avoid intermittent
  // failures due to the mock extensions not responding in time.
  Services.prefs.setIntPref("browser.urlbar.extension.timeout", 5000);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.extension.timeout");
  });

  await AddonTestUtils.promiseStartupManager();

  // Add a test engine and make it default so that when we do searches below,
  // Firefox doesn't try to include search suggestions from the actual default
  // engine from over the network.
  await SearchTestUtils.installSearchExtension(
    {
      name: "Test engine",
      keyword: "@testengine",
      search_url_get_params: "s={searchTerms}",
    },
    { setAsDefault: true }
  );
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

// Extensions must be privileged to use browser.urlbar.
add_task(async function test_urlbar_temporary_without_privilege() {
  let extension = ExtensionTestUtils.loadExtension({
    temporarilyInstalled: true,
    isPrivileged: false,
    manifest: {
      permissions: ["urlbar"],
    },
  });
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let { messages } = await promiseConsoleOutput(async () => {
    await Assert.rejects(
      extension.startup(),
      /Using the privileged permission/,
      "Startup failed with privileged permission"
    );
  });
  ExtensionTestUtils.failOnSchemaWarnings(true);
  AddonTestUtils.checkMessages(
    messages,
    {
      expected: [
        {
          message:
            /Using the privileged permission 'urlbar' requires a privileged add-on/,
        },
      ],
    },
    true
  );
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
            Array.isArray(query.sources),
            "sources is an array"
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
  // will set the provider's isActive and priority.
  let queryContext = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    searchString: "*",
  });
  let controller = UrlbarTestUtils.newMockController();
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
    if (restricting) {
      Assert.notEqual(
        provider.getPriority(queryContext),
        0,
        "Check provider priority"
      );
    } else {
      Assert.equal(
        provider.getPriority(queryContext),
        0,
        "Check provider priority"
      );
    }
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
// checks that the heuristic result from the built-in HeuristicFallback provider
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
        browser.test.assertTrue(Array.isArray(query.sources));
        return [
          {
            type: "remote_tab",
            source: "tabs",
            payload: {
              title: "Test remote_tab-tabs result",
              url: "https://example.com/remote_tab-tabs",
              device: "device",
              lastUsed: 1621366890,
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
              url: "https://example.com/tab-tabs",
            },
          },
          {
            type: "tip",
            source: "local",
            payload: {
              text: "Test tip-local result text",
              buttonText: "Test tip-local result button text",
              buttonUrl: "https://example.com/tip-button",
              helpUrl: "https://example.com/tip-help",
            },
          },
          {
            type: "url",
            source: "history",
            payload: {
              title: "Test url-history result",
              url: "https://example.com/url-history",
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
  let controller = UrlbarTestUtils.newMockController();
  await controller.startQuery(context);

  // Check isActive and priority.
  Assert.ok(provider.isActive(context));
  Assert.equal(provider.getPriority(context), 0);

  // Check the results.
  let expectedResults = [
    // The first result should be a search result returned by HeuristicFallback.
    {
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      title: "test",
      heuristic: true,
      payload: {
        query: "test",
        engine: "Test engine",
      },
    },
    // The second result should be our search suggestion result since the
    // default muxer sorts search suggestion results before other types.
    {
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      title: "Test search-search result",
      heuristic: false,
      payload: {
        engine: "Test engine",
        suggestion: "Test search-search result",
      },
    },
    // The rest of the results should appear in the order we returned them
    // above.
    {
      type: UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
      source: UrlbarUtils.RESULT_SOURCE.TABS,
      title: "Test remote_tab-tabs result",
      heuristic: false,
      payload: {
        title: "Test remote_tab-tabs result",
        url: "https://example.com/remote_tab-tabs",
        displayUrl: "example.com/remote_tab-tabs",
        device: "device",
        lastUsed: 1621366890,
      },
    },
    {
      type: UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      source: UrlbarUtils.RESULT_SOURCE.TABS,
      title: "Test tab-tabs result",
      heuristic: false,
      payload: {
        title: "Test tab-tabs result",
        url: "https://example.com/tab-tabs",
        displayUrl: "example.com/tab-tabs",
      },
    },
    {
      type: UrlbarUtils.RESULT_TYPE.TIP,
      source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      title: "",
      heuristic: false,
      payload: {
        text: "Test tip-local result text",
        buttonText: "Test tip-local result button text",
        buttonUrl: "https://example.com/tip-button",
        helpUrl: "https://example.com/tip-help",
        helpL10n: {
          id: "urlbar-result-menu-tip-get-help",
        },
        type: "extension",
      },
    },
    {
      type: UrlbarUtils.RESULT_TYPE.URL,
      source: UrlbarUtils.RESULT_SOURCE.HISTORY,
      title: "Test url-history result",
      heuristic: false,
      payload: {
        title: "Test url-history result",
        url: "https://example.com/url-history",
        displayUrl: "example.com/url-history",
      },
    },
  ];

  Assert.ok(context.results.every(r => !r.hasSuggestedIndex));
  let actualResults = context.results.map(r => ({
    type: r.type,
    source: r.source,
    title: r.title,
    heuristic: r.heuristic,
    payload: getPayload(r),
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
              url: "https://example.com/?s",
              suggestion: "url specified",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              engine: "Test engine",
              keyword: "@testengine",
              url: "https://example.com/?s",
              suggestion: "engine, keyword, and url specified",
            },
          },
          {
            type: "search",
            source: "search",
            payload: {
              keyword: "@testengine",
              url: "https://example.com/?s",
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
  let controller = UrlbarTestUtils.newMockController();
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
                url: `https://example.com/${behavior}`,
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
  let controller = UrlbarTestUtils.newMockController();
  await controller.startQuery(context);

  // Check isActive and priority.
  Assert.ok(active.isActive(context));
  Assert.ok(!inactive.isActive(context));
  Assert.equal(active.getPriority(context), 0);
  Assert.equal(inactive.getPriority(context), 0);

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
                url: `https://example.com/${i}`,
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
  let controller = UrlbarTestUtils.newMockController();
  await controller.startQuery(context);

  // Check isActive and priority.
  for (let provider of providers) {
    Assert.ok(provider.isActive(context));
    Assert.equal(provider.getPriority(context), 0);
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
  let controller = UrlbarTestUtils.newMockController();
  await controller.startQuery(context);

  // Check isActive and priority.
  for (let provider of providers) {
    Assert.ok(!provider.isActive(context));
    Assert.equal(provider.getPriority(context), 0);
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
                url: `https://example.com/${behavior}`,
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
  let controller = UrlbarTestUtils.newMockController();
  await controller.startQuery(context);

  // Check isActive and isRestricting.
  Assert.ok(providers.active.isActive(context));
  Assert.equal(providers.active.getPriority(context), 0);
  Assert.ok(!providers.inactive.isActive(context));
  Assert.equal(providers.inactive.getPriority(context), 0);
  Assert.ok(providers.restricting.isActive(context));
  Assert.notEqual(providers.restricting.getPriority(context), 0);

  // Check the results.
  Assert.equal(context.results.length, 1);
  Assert.equal(context.results[0].title, "Test result restricting");

  await ext.unload();
});

// Adds a restricting provider that returns a heuristic result.  The actual
// result created from the extension's result should be a heuristic.
add_task(async function test_heuristicRestricting() {
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
            type: "url",
            source: "history",
            heuristic: true,
            payload: {
              title: "Test result",
              url: "https://example.com/",
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
  let controller = UrlbarTestUtils.newMockController();
  await controller.startQuery(context);

  // Check the results.
  Assert.equal(context.results.length, 1);
  Assert.ok(context.results[0].heuristic);

  await ext.unload();
});

// Adds a non-restricting provider that returns a heuristic result.  The actual
// result created from the extension's result should *not* be a heuristic, and
// the usual UrlbarProviderHeuristicFallback heuristic should be present.
add_task(async function test_heuristicNonRestricting() {
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
        return [
          {
            type: "url",
            source: "history",
            heuristic: true,
            payload: {
              title: "Test result",
              url: "https://example.com/",
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
  let controller = UrlbarTestUtils.newMockController();
  await controller.startQuery(context);

  // Check the results.  The first result should be
  // UrlbarProviderHeuristicFallback's heuristic.
  let firstResult = context.results[0];
  Assert.ok(firstResult.heuristic);
  Assert.equal(firstResult.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(firstResult.source, UrlbarUtils.RESULT_SOURCE.SEARCH);
  Assert.equal(firstResult.payload.engine, "Test engine");

  // The extension result should be present but not the heuristic.
  let result = context.results.find(r => r.title == "Test result");
  Assert.ok(result);
  Assert.ok(!result.heuristic);

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
  let controller = UrlbarTestUtils.newMockController();
  await controller.startQuery(context);

  // Check isActive and isRestricting.
  Assert.ok(provider.isActive(context));
  Assert.equal(provider.getPriority(context), 0);

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
              url: "https://example.com/",
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
  let controller = UrlbarTestUtils.newMockController();
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
        browser.test.sendMessage("onBehaviorRequested");
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
  let controller = UrlbarTestUtils.newMockController();

  let startPromise = controller.startQuery(context);
  // Ensure the query has started, before trying to cancel it, otherwise we
  // won't get a `canceled` notification, as there's nothing to cancel.
  await ext.awaitMessage("onBehaviorRequested");
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
        // setTimeout is available in background scripts
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout, no-undef
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
  let controller = UrlbarTestUtils.newMockController();

  Services.prefs.setIntPref("browser.urlbar.extension.timeout", 0);
  await controller.startQuery(context);
  Services.prefs.clearUserPref("browser.urlbar.extension.timeout");

  // Check isActive and priority.
  Assert.ok(!provider.isActive(context));
  Assert.equal(provider.getPriority(context), 0);

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
        // setTimeout is available in background scripts
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout, no-undef
        await new Promise(r => setTimeout(r, 600));
        return [
          {
            type: "url",
            source: "history",
            payload: {
              title: "Test result",
              url: "https://example.com/",
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
  let controller = UrlbarTestUtils.newMockController();

  await controller.startQuery(context);

  // Check isActive and priority.
  Assert.ok(provider.isActive(context));
  Assert.equal(provider.getPriority(context), 0);

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
  // will set the provider's isActive and priority.
  let queryContext = new UrlbarQueryContext({
    allowAutofill: false,
    isPrivate: true,
    maxResults: 10,
    searchString: "*",
  });
  let controller = UrlbarTestUtils.newMockController();
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
  let controller = UrlbarTestUtils.newMockController();

  let startPromise = controller.startQuery(context);
  controller.cancelQuery();
  await startPromise;

  // Check isActive and priority.
  Assert.ok(!provider.isActive(context));
  Assert.equal(provider.getPriority(context), 0);

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
  let controller = UrlbarTestUtils.newMockController();
  await controller.startQuery(context);

  // Check isActive and priority.
  Assert.ok(provider.isActive(context));
  Assert.equal(provider.getPriority(context), 0);

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
  let controller = UrlbarTestUtils.newMockController();

  let startPromise = controller.startQuery(context);
  // Ensure the query has started, before trying to cancel it, otherwise we
  // won't get a `canceled` notification, as there's nothing to cancel.
  await ext.awaitMessage("onBehaviorRequested");

  controller.cancelQuery();
  await startPromise;

  // onQueryCanceled should have been fired.
  await ext.awaitMessage("onQueryCanceled");

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
              url: "https://example.com/",
            },
            suggestedIndex: 1,
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
  let controller = UrlbarTestUtils.newMockController();
  await controller.startQuery(context);

  // Check isActive and priority.
  Assert.ok(provider.isActive(context));
  Assert.equal(provider.getPriority(context), 0);

  // Check the results.
  Assert.equal(context.results.length, 2);
  Assert.ok(context.results[0].heuristic);
  Assert.equal(context.results[1].title, "Test result");
  Assert.equal(context.results[1].suggestedIndex, 1);

  await ext.unload();
});
