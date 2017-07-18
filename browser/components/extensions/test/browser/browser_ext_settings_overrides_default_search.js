/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

add_task(async function test_extension_setting_default_engine() {
  let defaultEngineName = Services.search.currentEngine.name;

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "DuckDuckGo",
          "search_url": "https://example.com/?q={searchTerms}",
          "is_default": true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine should be DuckDuckGo");

  await ext1.unload();

  is(Services.search.currentEngine.name, defaultEngineName, "Default engine should be " + defaultEngineName);
});

add_task(async function test_extension_setting_invalid_name_default_engine() {
  let defaultEngineName = Services.search.currentEngine.name;

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "InvalidName",
          "search_url": "https://example.com/?q={searchTerms}",
          "is_default": true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();

  is(Services.search.currentEngine.name, defaultEngineName, "Default engine should be " + defaultEngineName);

  await ext1.unload();

  is(Services.search.currentEngine.name, defaultEngineName, "Default engine should be " + defaultEngineName);
});

add_task(async function test_extension_setting_multiple_default_engine() {
  let defaultEngineName = Services.search.currentEngine.name;
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "DuckDuckGo",
          "search_url": "https://example.com/?q={searchTerms}",
          "is_default": true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  let ext2 = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "Twitter",
          "search_url": "https://example.com/?q={searchTerms}",
          "is_default": true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine should be DuckDuckGo");

  await ext2.startup();

  is(Services.search.currentEngine.name, "Twitter", "Default engine should be Twitter");

  await ext2.unload();

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine should be DuckDuckGo");

  await ext1.unload();

  is(Services.search.currentEngine.name, defaultEngineName, "Default engine should be " + defaultEngineName);
});

add_task(async function test_extension_setting_invalid_default_engine() {
  let defaultEngineName = Services.search.currentEngine.name;
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "MozSearch",
          "keyword": "MozSearch",
          "search_url": "https://example.com/?q={searchTerms}",
        },
      },
    },
    useAddonManager: "temporary",
  });

  let ext2 = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "MozSearch",
          "search_url": "https://example.com/?q={searchTerms}",
          "is_default": true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();

  is(Services.search.currentEngine.name, defaultEngineName, "Default engine should be " + defaultEngineName);

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

  await ext2.startup();

  is(Services.search.currentEngine.name, defaultEngineName, "Default engine should be " + defaultEngineName);

  await ext2.unload();

  is(Services.search.currentEngine.name, defaultEngineName, "Default engine should be " + defaultEngineName);

  await ext1.unload();

  is(Services.search.currentEngine.name, defaultEngineName, "Default engine should be " + defaultEngineName);
});
