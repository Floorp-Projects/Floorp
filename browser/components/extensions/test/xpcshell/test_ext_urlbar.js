"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarQueryContext: "resource:///modules/UrlbarUtils.jsm",
});

add_task(async function test_urlbar_without_urlbar_permission() {
  let ext = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    background() {
      browser.test.assertEq(browser.urlbar, undefined,
                            "'urlbar' permission is required");
    },
  });
  await ext.startup();
  await ext.unload();
});

add_task(async function test_urlbar_no_privilege() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    background() {
      browser.test.assertEq(browser.urlbar, undefined,
                            "'urlbar' permission is privileged");
    },
  });
  await ext.startup();
  await ext.unload();
});

add_task(async function test_privateBrowsing_not_allowed() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
      incognito: "not_allowed",
    },
    isPrivileged: true,
    background() {
      browser.urlbar.onQueryReady.addListener(queryContext => {
        browser.test.notifyFail("urlbar called in private browsing");
        return "active";
      }, "Test-private");
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
  await UrlbarProvidersManager.startQuery(queryContext, controller);
  // Check the providers behavior has been setup properly.
  let provider = UrlbarProvidersManager.providers
                                       .find(p => p.name == "Test-private");
  Assert.ok(!provider.isActive({}), "Check provider is inactive");

  await ext.unload();
});

add_task(async function test_privateBrowsing_allowed() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    incognitoOverride: "spanning",
    background() {
      browser.urlbar.onQueryReady.addListener(queryContext => {
        return "active";
      }, "Test-private");
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
  await UrlbarProvidersManager.startQuery(queryContext, controller);
  // Check the providers behavior has been setup properly.
  let provider = UrlbarProvidersManager.providers
                                       .find(p => p.name == "Test-private");
  Assert.ok(provider.isActive({}), "Check provider is active");

  await ext.unload();
});

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
        browser.urlbar.onQueryReady.addListener(queryContext => {
          browser.test.assertFalse(queryContext.isPrivate,
                                   "Context is non private");
          browser.test.assertEq(queryContext.maxResults, 10,
                                "Check maxResults");
          browser.test.assertTrue(queryContext.searchString,
                                  "SearchString is non empty");
          browser.test.assertTrue(Array.isArray(queryContext.acceptableSources),
                                  "acceptableSources is an array");
          return state;
        }, name);
      }
    },
  });
  await ext.startup();

  Assert.greater(UrlbarProvidersManager.providers.length, providers.length,
                 "Providers have been added");

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
  await UrlbarProvidersManager.startQuery(queryContext, controller);
  // Check the providers behavior has been setup properly.
  for (let provider of UrlbarProvidersManager.providers) {
    if (!provider.name.startsWith("Test")) {
      continue;
    }
    let [, state] = provider.name.split("-");
    let isActive = state != "inactive";
    let restricting = state == "restricting";
    Assert.equal(isActive, provider.isActive(queryContext),
                 "Check active callback");
    Assert.equal(restricting, provider.isRestricting(queryContext),
                 "Check restrict callback");
  }

  await ext.unload();

  // Sanity check the providers.
  Assert.deepEqual(UrlbarProvidersManager.providers, providers,
                   "Should return to the default providers");
});
