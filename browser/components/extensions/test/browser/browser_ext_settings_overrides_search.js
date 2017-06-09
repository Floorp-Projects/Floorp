/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

add_task(async function test_extension_adding_engine() {
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

  await ext1.startup();

  let engine = Services.search.getEngineByName("MozSearch");
  ok(engine, "Engine should exist.");

  await ext1.unload();

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

  let ext2 = ExtensionTestUtils.loadExtension({
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

  await ext1.startup();

  let engine = Services.search.getEngineByName("MozSearch");
  Services.search.currentEngine = engine;
  Services.search.moveEngine(engine, 1);

  await ext2.startup();

  engine = Services.search.getEngineByName("MozSearch");
  is(Services.search.currentEngine, engine, "Default engine should still be MozSearch");
  is(Services.search.getEngines().indexOf(engine), 1, "Engine is in position 1");

  await ext2.unload();
  await ext1.unload();

  engine = Services.search.getEngineByName("MozSearch");
  ok(!engine, "Engine should not exist");
});

