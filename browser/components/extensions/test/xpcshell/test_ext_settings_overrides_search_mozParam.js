/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

let {
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

add_task(async function setup() {
  await promiseStartupManager();
  await Services.search.init();
  registerCleanupFunction(async () => {
    await promiseShutdownManager();
  });
});

/* This tests setting moz params. */
add_task(async function test_extension_setting_moz_params() {
  let defaultBranch = Services.prefs.getDefaultBranch("browser.search.");
  defaultBranch.setCharPref("param.code", "good");
  Services.prefs.setCharPref("extensions.installedDistroAddon.test@mochitest", "foo");
  // These params are conditional based on how search is initiated.
  let mozParams = [
    {name: "test-0", condition: "purpose", purpose: "contextmenu", value: "0"},
    {name: "test-1", condition: "purpose", purpose: "searchbar", value: "1"},
    {name: "test-2", condition: "purpose", purpose: "homepage", value: "2"},
    {name: "test-3", condition: "purpose", purpose: "keyword", value: "3"},
    {name: "test-4", condition: "purpose", purpose: "newtab", value: "4"},
  ];
  // These params are always included.
  let params = [
    {name: "simple", value: "5"},
    {name: "term", value: "{searchTerms}"},
    {name: "lang", value: "{language}"},
    {name: "locale", value: "{moz:locale}"},
    {name: "prefval", condition: "pref", pref: "code"},
  ];

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "applications": {
        "gecko": {"id": "test@mochitest"},
      },
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "MozParamsTest",
          "search_url": "https://example.com/?q={searchTerms}",
          "params": [...mozParams, ...params],
        },
      },
    },
    useAddonManager: "permanent",
  });
  await extension.startup();
  await AddonTestUtils.waitForSearchProviderStartup(extension);
  equal(extension.extension.isPrivileged, true, "extension is priviledged");

  let engine = Services.search.getEngineByName("MozParamsTest");

  let extraParams = [];
  for (let p of params) {
    if (p.condition == "pref") {
      extraParams.push(`${p.name}=good`);
    } else if (p.value == "{searchTerms}") {
      extraParams.push(`${p.name}=test`);
    } else if (p.value == "{language}") {
      extraParams.push(`${p.name}=${Services.locale.requestedLocale || "*"}`);
    } else if (p.value == "{moz:locale}") {
      extraParams.push(`${p.name}=${Services.locale.requestedLocale}`);
    } else {
      extraParams.push(`${p.name}=${p.value}`);
    }
  }
  let paramStr = extraParams.join("&");

  for (let p of mozParams) {
    let expectedURL = engine.getSubmission("test", null, p.condition == "purpose" ? p.purpose : null).uri.spec;
    equal(expectedURL, `https://example.com/?q=test&${p.name}=${p.value}&${paramStr}`, "search url is expected");
  }

  await extension.unload();
});

add_task(async function test_extension_setting_moz_params_fail() {
  // Ensure that the test infra does not automatically make
  // this privileged.
  AddonTestUtils.usePrivilegedSignatures = false;
  Services.prefs.setCharPref("extensions.installedDistroAddon.test@mochitest", "");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "applications": {
        "gecko": {"id": "test@mochitest"},
      },
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "MozParamsTest",
          "search_url": "https://example.com/",
          "params": [
            {name: "testParam", condition: "purpose", purpose: "contextmenu", value: "0"},
            {name: "prefval", condition: "pref", pref: "code"},
            {name: "q", value: "{searchTerms}"},
          ],
        },
      },
    },
    useAddonManager: "permanent",
  });
  await extension.startup();
  await AddonTestUtils.waitForSearchProviderStartup(extension);
  equal(extension.extension.isPrivileged, false, "extension is not priviledged");
  let engine = Services.search.getEngineByName("MozParamsTest");
  let expectedURL = engine.getSubmission("test", null, "contextmenu").uri.spec;
  equal(expectedURL, "https://example.com/?q=test", "engine cannot have conditional or pref params");
  await extension.unload();
});
