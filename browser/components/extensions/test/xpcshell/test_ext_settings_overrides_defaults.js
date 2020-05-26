/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const { SearchTestUtils } = ChromeUtils.import(
  "resource://testing-common/SearchTestUtils.jsm"
);

const { SearchUtils } = ChromeUtils.import(
  "resource://gre/modules/SearchUtils.jsm"
);

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

const kSearchEngineURL = "https://example.com/?q={searchTerms}&foo=myparams";
const kSuggestURL = "https://example.com/fake/suggest/";
const kSuggestURLParams = "q={searchTerms}&type=list2";

Services.prefs.setBoolPref("browser.search.log", true);

add_task(async function setup() {
  AddonTestUtils.usePrivilegedSignatures = false;
  AddonTestUtils.overrideCertDB();
  await AddonTestUtils.promiseStartupManager();
  await SearchTestUtils.useTestEngines("data", null, [
    {
      webExtension: {
        id: "test@search.mozilla.org",
      },
      appliesTo: [
        {
          included: { everywhere: true },
          default: "yes",
        },
      ],
    },
    {
      webExtension: {
        id: "test2@search.mozilla.org",
      },
      appliesTo: [
        {
          included: { everywhere: true },
        },
      ],
    },
  ]);
  await Services.search.init();
  registerCleanupFunction(async () => {
    await AddonTestUtils.promiseShutdownManager();
  });
});

add_task(async function test_extension_changing_to_app_provided_default() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      icons: {
        "16": "foo.ico",
      },
      chrome_settings_overrides: {
        search_provider: {
          is_default: true,
          name: "MozParamsTest2",
          keyword: "MozSearch",
          search_url: kSearchEngineURL,
          suggest_url: kSuggestURL,
          suggest_url_get_params: kSuggestURLParams,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  Assert.equal(
    Services.search.defaultEngine.name,
    "MozParamsTest2",
    "Should have switched the default engine."
  );

  let engine = Services.search.getEngineByName("MozParamsTest2");
  Assert.ok(engine, "Should have found the engine");

  Assert.equal(
    engine.getSubmission("{searchTerms}").uri.spec,
    encodeURI("https://example.com/2/?q={searchTerms}&simple2=5"),
    "Should have not changed the submission url."
  );
  Assert.equal(
    engine.getSubmission("{searchTerms}", URLTYPE_SUGGEST_JSON),
    null,
    "Should not have set a suggestion URL."
  );

  let promiseDefaultBrowserChange = SearchTestUtils.promiseSearchNotification(
    "engine-default",
    "browser-search-engine-modified"
  );
  await ext1.unload();
  await promiseDefaultBrowserChange;

  Assert.equal(
    Services.search.defaultEngine.name,
    "MozParamsTest",
    "Should have reverted to the original default engine."
  );
});

add_task(async function test_extension_overriding_app_provided_default() {
  const settings = await RemoteSettings(SearchUtils.SETTINGS_ALLOWLIST_KEY);
  sinon.stub(settings, "get").returns([
    {
      thirdPartyId: "test@thirdparty.example.com",
      overridesId: "test2@search.mozilla.org",
      urls: [
        {
          search_url: "https://example.com/?q={searchTerms}&foo=myparams",
        },
      ],
    },
  ]);

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "test@thirdparty.example.com",
        },
      },
      icons: {
        "16": "foo.ico",
      },
      chrome_settings_overrides: {
        search_provider: {
          is_default: true,
          name: "MozParamsTest2",
          keyword: "MozSearch",
          search_url: kSearchEngineURL,
          suggest_url: kSuggestURL,
          suggest_url_get_params: kSuggestURLParams,
        },
      },
    },
    useAddonManager: "permanent",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  Assert.equal(
    Services.search.defaultEngine.name,
    "MozParamsTest2",
    "Should have switched the default engine."
  );

  let engine = Services.search.getEngineByName("MozParamsTest2");
  Assert.ok(engine, "Should have found the engine");

  Assert.equal(
    engine.getSubmission("{searchTerms}").uri.spec,
    encodeURI(kSearchEngineURL),
    "Should have changed the suggestion url."
  );
  Assert.equal(
    engine.getSubmission("{searchTerms}", URLTYPE_SUGGEST_JSON).uri.spec,
    encodeURI(`${kSuggestURL}?${kSuggestURLParams}`),
    "Should set a submission URL."
  );

  let promiseDefaultBrowserChange = SearchTestUtils.promiseSearchNotification(
    "engine-default",
    "browser-search-engine-modified"
  );
  await ext1.unload();
  await promiseDefaultBrowserChange;

  Assert.equal(
    Services.search.defaultEngine.name,
    "MozParamsTest",
    "Should have reverted to the original default engine."
  );

  engine = Services.search.getEngineByName("MozParamsTest2");
  Assert.ok(engine, "Should have found the engine");

  Assert.equal(
    engine.getSubmission("{searchTerms}").uri.spec,
    encodeURI("https://example.com/2/?q={searchTerms}&simple2=5"),
    "Should have reverted the submission url."
  );
  Assert.equal(
    engine.getSubmission("{searchTerms}", URLTYPE_SUGGEST_JSON),
    null,
    "Should have reverted the suggestion URL."
  );
});
