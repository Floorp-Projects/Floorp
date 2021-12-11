/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

let delay = () => new Promise(resolve => setTimeout(resolve, 0));

const kSearchFormURL = "https://example.com/searchform";
const kSearchEngineURL = "https://example.com/?search={searchTerms}";
const kSearchSuggestURL = "https://example.com/?suggest={searchTerms}";
const kSearchTerm = "foo";
const kSearchTermIntl = "日";
const URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);
// Override ExtensionXPCShellUtils.jsm's overriding of the pref as the
// search service needs it.
Services.prefs.clearUserPref("services.settings.default_bucket");

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_extension_adding_engine() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      icons: {
        "16": "foo.ico",
        "32": "foo32.ico",
      },
      chrome_settings_overrides: {
        search_provider: {
          name: "MozSearch",
          keyword: "MozSearch",
          search_form: kSearchFormURL,
          search_url: kSearchEngineURL,
          suggest_url: kSearchSuggestURL,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

  let { baseURI } = ext1.extension;
  equal(engine.iconURI.spec, baseURI.resolve("foo.ico"), "icon path matches");
  let icons = engine.getIcons();
  equal(icons.length, 2, "both icons avialable");
  equal(icons[0].url, baseURI.resolve("foo.ico"), "icon path matches");
  equal(icons[1].url, baseURI.resolve("foo32.ico"), "icon path matches");

  let expectedSuggestURL = kSearchSuggestURL.replace(
    "{searchTerms}",
    kSearchTerm
  );
  let submissionSuggest = engine.getSubmission(
    kSearchTerm,
    URLTYPE_SUGGEST_JSON
  );
  let encodedSubmissionURL = engine.getSubmission(kSearchTermIntl).uri.spec;
  let testSubmissionURL = kSearchEngineURL.replace(
    "{searchTerms}",
    encodeURIComponent(kSearchTermIntl)
  );
  equal(
    encodedSubmissionURL,
    testSubmissionURL,
    "Encoded UTF-8 URLs should match"
  );

  equal(
    submissionSuggest.uri.spec,
    expectedSuggestURL,
    "Suggest URLs should match"
  );

  equal(engine.searchForm, kSearchFormURL, "Search form URLs should match");
  await ext1.unload();
  await delay();

  engine = Services.search.getEngineByName("MozSearch");
  ok(!engine, "Engine should not exist");
});

add_task(async function test_extension_adding_engine_with_spaces() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: "MozSearch     ",
          keyword: "MozSearch",
          search_url: "https://example.com/?q={searchTerms}",
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

  await ext1.unload();
  await delay();

  engine = Services.search.getEngineByName("MozSearch");
  ok(!engine, "Engine should not exist");
});

add_task(async function test_upgrade_default_position_engine() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: "MozSearch",
          keyword: "MozSearch",
          search_url: "https://example.com/?q={searchTerms}",
        },
      },
      applications: {
        gecko: {
          id: "testengine@mozilla.com",
        },
      },
      version: "0.1",
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  let engine = Services.search.getEngineByName("MozSearch");
  await Services.search.setDefault(engine);
  await Services.search.moveEngine(engine, 1);

  await ext1.upgrade({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: "MozSearch",
          keyword: "MozSearch",
          search_url: "https://example.com/?q={searchTerms}",
        },
      },
      applications: {
        gecko: {
          id: "testengine@mozilla.com",
        },
      },
      version: "0.2",
    },
    useAddonManager: "temporary",
  });
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  engine = Services.search.getEngineByName("MozSearch");
  equal(
    Services.search.defaultEngine,
    engine,
    "Default engine should still be MozSearch"
  );
  equal(
    (await Services.search.getEngines()).map(e => e.name).indexOf(engine.name),
    1,
    "Engine is in position 1"
  );

  await ext1.unload();
  await delay();

  engine = Services.search.getEngineByName("MozSearch");
  ok(!engine, "Engine should not exist");
});

add_task(async function test_extension_get_params() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: "MozSearch",
          keyword: "MozSearch",
          search_url: kSearchEngineURL,
          search_url_get_params: "foo=bar&bar=foo",
          suggest_url: kSearchSuggestURL,
          suggest_url_get_params: "foo=bar&bar=foo",
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

  let url = engine.wrappedJSObject._getURLOfType("text/html");
  equal(url.method, "GET", "Search URLs method is GET");

  let expectedURL = kSearchEngineURL.replace("{searchTerms}", kSearchTerm);
  let submission = engine.getSubmission(kSearchTerm);
  equal(
    submission.uri.spec,
    `${expectedURL}&foo=bar&bar=foo`,
    "Search URLs should match"
  );

  let expectedSuggestURL = kSearchSuggestURL.replace(
    "{searchTerms}",
    kSearchTerm
  );
  let submissionSuggest = engine.getSubmission(
    kSearchTerm,
    URLTYPE_SUGGEST_JSON
  );
  equal(
    submissionSuggest.uri.spec,
    `${expectedSuggestURL}&foo=bar&bar=foo`,
    "Suggest URLs should match"
  );

  await ext1.unload();
});

add_task(async function test_extension_post_params() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: "MozSearch",
          keyword: "MozSearch",
          search_url: kSearchEngineURL,
          search_url_post_params: "foo=bar&bar=foo",
          suggest_url: kSearchSuggestURL,
          suggest_url_post_params: "foo=bar&bar=foo",
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

  let url = engine.wrappedJSObject._getURLOfType("text/html");
  equal(url.method, "POST", "Search URLs method is POST");

  let expectedURL = kSearchEngineURL.replace("{searchTerms}", kSearchTerm);
  let submission = engine.getSubmission(kSearchTerm);
  equal(submission.uri.spec, expectedURL, "Search URLs should match");
  // postData is a nsIMIMEInputStream which contains a nsIStringInputStream.
  equal(
    submission.postData.data.data,
    "foo=bar&bar=foo",
    "Search postData should match"
  );

  let expectedSuggestURL = kSearchSuggestURL.replace(
    "{searchTerms}",
    kSearchTerm
  );
  let submissionSuggest = engine.getSubmission(
    kSearchTerm,
    URLTYPE_SUGGEST_JSON
  );
  equal(
    submissionSuggest.uri.spec,
    expectedSuggestURL,
    "Suggest URLs should match"
  );
  equal(
    submissionSuggest.postData.data.data,
    "foo=bar&bar=foo",
    "Suggest postData should match"
  );

  await ext1.unload();
});

add_task(async function test_extension_no_query_params() {
  const ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: "MozSearch",
          keyword: "MozSearch",
          search_url: "https://example.com/{searchTerms}",
          suggest_url: "https://example.com/suggest/{searchTerms}",
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

  const encodedSubmissionURL = engine.getSubmission(kSearchTermIntl).uri.spec;
  const testSubmissionURL =
    "https://example.com/" + encodeURIComponent(kSearchTermIntl);
  equal(
    encodedSubmissionURL,
    testSubmissionURL,
    "Encoded UTF-8 URLs should match"
  );

  const expectedSuggestURL = "https://example.com/suggest/" + kSearchTerm;
  let submissionSuggest = engine.getSubmission(
    kSearchTerm,
    URLTYPE_SUGGEST_JSON
  );
  equal(
    submissionSuggest.uri.spec,
    expectedSuggestURL,
    "Suggest URLs should match"
  );

  await ext1.unload();
  await delay();

  engine = Services.search.getEngineByName("MozSearch");
  ok(!engine, "Engine should not exist");
});

add_task(async function test_extension_empty_suggestUrl() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      default_locale: "en",
      chrome_settings_overrides: {
        search_provider: {
          name: "MozSearch",
          keyword: "MozSearch",
          search_url: kSearchEngineURL,
          search_url_post_params: "foo=bar&bar=foo",
          suggest_url: "__MSG_suggestUrl__",
          suggest_url_get_params: "__MSG_suggestUrlGetParams__",
        },
      },
    },
    useAddonManager: "temporary",
    files: {
      "_locales/en/messages.json": {
        suggestUrl: {
          message: "",
        },
        suggestUrlGetParams: {
          message: "",
        },
      },
    },
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

  let url = engine.wrappedJSObject._getURLOfType("text/html");
  equal(url.method, "POST", "Search URLs method is POST");

  let expectedURL = kSearchEngineURL.replace("{searchTerms}", kSearchTerm);
  let submission = engine.getSubmission(kSearchTerm);
  equal(submission.uri.spec, expectedURL, "Search URLs should match");
  // postData is a nsIMIMEInputStream which contains a nsIStringInputStream.
  equal(
    submission.postData.data.data,
    "foo=bar&bar=foo",
    "Search postData should match"
  );

  let submissionSuggest = engine.getSubmission(
    kSearchTerm,
    URLTYPE_SUGGEST_JSON
  );
  ok(!submissionSuggest, "There should be no suggest URL.");

  await ext1.unload();
});

add_task(async function test_extension_empty_suggestUrl_with_params() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      default_locale: "en",
      chrome_settings_overrides: {
        search_provider: {
          name: "MozSearch",
          keyword: "MozSearch",
          search_url: kSearchEngineURL,
          search_url_post_params: "foo=bar&bar=foo",
          suggest_url: "__MSG_suggestUrl__",
          suggest_url_get_params: "__MSG_suggestUrlGetParams__",
        },
      },
    },
    useAddonManager: "temporary",
    files: {
      "_locales/en/messages.json": {
        suggestUrl: {
          message: "",
        },
        suggestUrlGetParams: {
          message: "abc",
        },
      },
    },
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

  let url = engine.wrappedJSObject._getURLOfType("text/html");
  equal(url.method, "POST", "Search URLs method is POST");

  let expectedURL = kSearchEngineURL.replace("{searchTerms}", kSearchTerm);
  let submission = engine.getSubmission(kSearchTerm);
  equal(submission.uri.spec, expectedURL, "Search URLs should match");
  // postData is a nsIMIMEInputStream which contains a nsIStringInputStream.
  equal(
    submission.postData.data.data,
    "foo=bar&bar=foo",
    "Search postData should match"
  );

  let submissionSuggest = engine.getSubmission(
    kSearchTerm,
    URLTYPE_SUGGEST_JSON
  );
  ok(!submissionSuggest, "There should be no suggest URL.");

  await ext1.unload();
});

async function checkBadUrl(searchProviderKey, urlValue) {
  let normalized = await ExtensionTestUtils.normalizeManifest({
    chrome_settings_overrides: {
      search_provider: {
        name: "MozSearch",
        keyword: "MozSearch",
        search_url: "https://example.com/",
        [searchProviderKey]: urlValue,
      },
    },
  });

  ok(
    /Error processing chrome_settings_overrides\.search_provider[^:]*: .* must match/.test(
      normalized.error
    ),
    `Expected error for ${searchProviderKey}:${urlValue} "${normalized.error}"`
  );
}

async function checkValidUrl(urlValue) {
  let normalized = await ExtensionTestUtils.normalizeManifest({
    chrome_settings_overrides: {
      search_provider: {
        name: "MozSearch",
        keyword: "MozSearch",
        search_form: urlValue,
        search_url: urlValue,
        suggest_url: urlValue,
      },
    },
  });
  equal(normalized.error, undefined, `Valid search_provider url: ${urlValue}`);
}

add_task(async function test_extension_not_allow_http() {
  await checkBadUrl("search_form", "http://example.com/{searchTerms}");
  await checkBadUrl("search_url", "http://example.com/{searchTerms}");
  await checkBadUrl("suggest_url", "http://example.com/{searchTerms}");
});

add_task(async function test_manifest_disallows_http_localhost_prefix() {
  await checkBadUrl("search_url", "http://localhost.example.com");
  await checkBadUrl("search_url", "http://localhost.example.com/");
  await checkBadUrl("search_url", "http://127.0.0.1.example.com/");
  await checkBadUrl("search_url", "http://localhost:1234@example.com/");
});

add_task(async function test_manifest_allow_http_for_localhost() {
  await checkValidUrl("http://localhost");
  await checkValidUrl("http://localhost/");
  await checkValidUrl("http://localhost:/");
  await checkValidUrl("http://localhost:1/");
  await checkValidUrl("http://localhost:65535/");

  await checkValidUrl("http://127.0.0.1");
  await checkValidUrl("http://127.0.0.1:");
  await checkValidUrl("http://127.0.0.1:/");
  await checkValidUrl("http://127.0.0.1/");
  await checkValidUrl("http://127.0.0.1:80/");

  await checkValidUrl("http://[::1]");
  await checkValidUrl("http://[::1]:");
  await checkValidUrl("http://[::1]:/");
  await checkValidUrl("http://[::1]/");
  await checkValidUrl("http://[::1]:80/");
});

add_task(async function test_extension_allow_http_for_localhost() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: "MozSearch",
          keyword: "MozSearch",
          search_url: "http://localhost/{searchTerms}",
          suggest_url: "http://localhost/suggest/{searchTerms}",
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

  await ext1.unload();
});

add_task(async function test_search_favicon_mv3() {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  let normalized = await ExtensionTestUtils.normalizeManifest({
    manifest_version: 3,
    chrome_settings_overrides: {
      search_provider: {
        name: "HTTP Icon in MV3",
        search_url: "https://example.org/",
        favicon_url: "https://example.org/icon.png",
      },
    },
  });
  Assert.ok(
    normalized.error.endsWith("must be a relative URL"),
    "Should have an error"
  );
  normalized = await ExtensionTestUtils.normalizeManifest({
    manifest_version: 3,
    chrome_settings_overrides: {
      search_provider: {
        name: "HTTP Icon in MV3",
        search_url: "https://example.org/",
        favicon_url: "/icon.png",
      },
    },
  });
  Assert.ok(!normalized.error, "Should not have an error");
});
