/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");

let delay = () => new Promise(resolve => setTimeout(resolve, 0));

const kSearchEngineURL = "https://example.com/?search={searchTerms}";
const kSearchSuggestURL = "http://example.com/?suggest={searchTerms}";
const kSearchTerm = "foo";
const kSearchTermIntl = "日";
const URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  Services.search.init();
});

add_task(async function test_extension_adding_engine() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
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

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

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

  let engine = Services.search.getEngineByName("MozSearch");
  Services.search.currentEngine = engine;
  Services.search.moveEngine(engine, 1);

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

  engine = Services.search.getEngineByName("MozSearch");
  equal(Services.search.currentEngine, engine, "Default engine should still be MozSearch");
  equal(Services.search.getEngines().indexOf(engine), 1, "Engine is in position 1");

  await ext1.unload();
  await delay();

  engine = Services.search.getEngineByName("MozSearch");
  ok(!engine, "Engine should not exist");
});
