/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
const {setTimeout} = ChromeUtils.import("resource://gre/modules/Timer.jsm");

let delay = () => new Promise(resolve => setTimeout(resolve, 0));

const kSearchEngineURL = "https://example.com/?search={searchTerms}";
const kSearchSuggestURL = "https://example.com/?suggest={searchTerms}";
const kSearchTerm = "foo";
const kSearchTermIntl = "日";
const URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_extension_adding_engine() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      "icons": {
        "16": "foo.ico",
        "32": "foo32.ico",
      },
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "MozSearch",
          "keyword": "MozSearch",
          "search_url": kSearchEngineURL,
          "suggest_url": kSearchSuggestURL,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

  let {baseURI} = ext1.extension;
  equal(engine.iconURI.spec, baseURI.resolve("foo.ico"), "icon path matches");
  let icons = engine.getIcons();
  equal(icons.length, 2, "both icons avialable");
  equal(icons[0].url, baseURI.resolve("foo.ico"), "icon path matches");
  equal(icons[1].url, baseURI.resolve("foo32.ico"), "icon path matches");

  let expectedSuggestURL = kSearchSuggestURL.replace("{searchTerms}", kSearchTerm);
  let submissionSuggest = engine.getSubmission(kSearchTerm, URLTYPE_SUGGEST_JSON);
  let encodedSubmissionURL = engine.getSubmission(kSearchTermIntl).uri.spec;
  let testSubmissionURL = kSearchEngineURL.replace("{searchTerms}", encodeURIComponent(kSearchTermIntl));
  equal(encodedSubmissionURL, testSubmissionURL, "Encoded UTF-8 URLs should match");

  equal(submissionSuggest.uri.spec, expectedSuggestURL, "Suggest URLs should match");

  await ext1.unload();
  await delay();

  engine = Services.search.getEngineByName("MozSearch");
  ok(!engine, "Engine should not exist");
});

add_task(async function test_extension_adding_engine_with_spaces() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "MozSearch     ",
          "keyword": "MozSearch",
          "search_url": "https://example.com/?q={searchTerms}",
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
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "MozSearch",
          "keyword": "MozSearch",
          "search_url": "https://example.com/?q={searchTerms}",
        },
      },
      "applications": {
        "gecko": {
          "id": "testengine@mozilla.com",
        },
      },
      "version": "0.1",
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
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "MozSearch",
          "keyword": "MozSearch",
          "search_url": "https://example.com/?q={searchTerms}",
        },
      },
      "applications": {
        "gecko": {
          "id": "testengine@mozilla.com",
        },
      },
      "version": "0.2",
    },
    useAddonManager: "temporary",
  });
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  engine = Services.search.getEngineByName("MozSearch");
  equal(Services.search.defaultEngine, engine, "Default engine should still be MozSearch");
  equal((await Services.search.getEngines()).map(e => e.name).indexOf(engine.name),
        1, "Engine is in position 1");

  await ext1.unload();
  await delay();

  engine = Services.search.getEngineByName("MozSearch");
  ok(!engine, "Engine should not exist");
});

add_task(async function test_extension_post_params() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "MozSearch",
          "keyword": "MozSearch",
          "search_url": kSearchEngineURL,
          "search_url_post_params": "foo=bar&bar=foo",
          "suggest_url": kSearchSuggestURL,
          "suggest_url_post_params": "foo=bar&bar=foo",
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
  equal(submission.postData.data.data, "foo=bar&bar=foo", "Search postData should match");

  let expectedSuggestURL = kSearchSuggestURL.replace("{searchTerms}", kSearchTerm);
  let submissionSuggest = engine.getSubmission(kSearchTerm, URLTYPE_SUGGEST_JSON);
  equal(submissionSuggest.uri.spec, expectedSuggestURL, "Suggest URLs should match");
  equal(submissionSuggest.postData.data.data, "foo=bar&bar=foo", "Suggest postData should match");

  await ext1.unload();
});

add_task(async function test_extension_no_query_params() {
  const ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "MozSearch",
          "keyword": "MozSearch",
          "search_url": "https://example.com/{searchTerms}",
          "suggest_url": "https://example.com/suggest/{searchTerms}",
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
  const testSubmissionURL = "https://example.com/" + encodeURIComponent(kSearchTermIntl);
  equal(encodedSubmissionURL, testSubmissionURL, "Encoded UTF-8 URLs should match");

  const expectedSuggestURL = "https://example.com/suggest/" + kSearchTerm;
  let submissionSuggest = engine.getSubmission(kSearchTerm, URLTYPE_SUGGEST_JSON);
  equal(submissionSuggest.uri.spec, expectedSuggestURL, "Suggest URLs should match");

  await ext1.unload();
  await delay();

  engine = Services.search.getEngineByName("MozSearch");
  ok(!engine, "Engine should not exist");
});
