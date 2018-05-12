/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");

const EXTENSION1_ID = "extension1@mozilla.com";
const EXTENSION2_ID = "extension2@mozilla.com";

let defaultEngineName = Services.search.currentEngine.name;

function restoreDefaultEngine() {
  let engine = Services.search.getEngineByName(defaultEngineName);
  Services.search.currentEngine = engine;
}
registerCleanupFunction(restoreDefaultEngine);

/* This tests setting a default engine. */
add_task(async function test_extension_setting_default_engine() {
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

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext1.unload();

  is(Services.search.currentEngine.name, defaultEngineName, `Default engine is ${defaultEngineName}`);
});

/* This tests that uninstalling add-ons maintains the proper
 * search default. */
add_task(async function test_extension_setting_multiple_default_engine() {
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

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext2.startup();

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");

  await ext2.unload();

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext1.unload();

  is(Services.search.currentEngine.name, defaultEngineName, `Default engine is ${defaultEngineName}`);
});

/* This tests that uninstalling add-ons in reverse order maintains the proper
 * search default. */
add_task(async function test_extension_setting_multiple_default_engine_reversed() {
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

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext2.startup();

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");

  await ext1.unload();

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");

  await ext2.unload();

  is(Services.search.currentEngine.name, defaultEngineName, `Default engine is ${defaultEngineName}`);
});

/* This tests that when the user changes the search engine and the add-on
 * is unistalled, search stays with the user's choice. */
add_task(async function test_user_changing_default_engine() {
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

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");

  let engine = Services.search.getEngineByName("Twitter");
  Services.search.currentEngine = engine;

  await ext1.unload();

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");
  restoreDefaultEngine();
});

/* This tests that when the user changes the search engine while it is
 * disabled, user choice is maintained when the add-on is reenabled. */
add_task(async function test_user_change_with_disabling() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: EXTENSION1_ID,
        },
      },
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

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");

  let engine = Services.search.getEngineByName("Twitter");
  Services.search.currentEngine = engine;

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");

  let disabledPromise = awaitEvent("shutdown", EXTENSION1_ID);
  let addon = await AddonManager.getAddonByID(EXTENSION1_ID);
  await addon.disable();
  await disabledPromise;

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");

  let enabledPromise = awaitEvent("ready", EXTENSION1_ID);
  await addon.enable();
  await enabledPromise;

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");
  await ext1.unload();
  restoreDefaultEngine();
});

/* This tests that when two add-ons are installed that change default
 * search and the first one is disabled, before the second one is installed,
 * when the first one is reenabled, the second add-on keeps the search. */
add_task(async function test_two_addons_with_first_disabled_before_second() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: EXTENSION1_ID,
        },
      },
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
      applications: {
        gecko: {
          id: EXTENSION2_ID,
        },
      },
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

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");

  let disabledPromise = awaitEvent("shutdown", EXTENSION1_ID);
  let addon1 = await AddonManager.getAddonByID(EXTENSION1_ID);
  await addon1.disable();
  await disabledPromise;

  is(Services.search.currentEngine.name, defaultEngineName, `Default engine is ${defaultEngineName}`);

  await ext2.startup();

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");

  let enabledPromise = awaitEvent("ready", EXTENSION1_ID);
  await addon1.enable();
  await enabledPromise;

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");
  await ext2.unload();

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");
  await ext1.unload();

  is(Services.search.currentEngine.name, defaultEngineName, `Default engine is ${defaultEngineName}`);
});

/* This tests that when two add-ons are installed that change default
 * search and the first one is disabled, the second one maintains
 * the search. */
add_task(async function test_two_addons_with_first_disabled() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: EXTENSION1_ID,
        },
      },
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
      applications: {
        gecko: {
          id: EXTENSION2_ID,
        },
      },
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

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext2.startup();

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");

  let disabledPromise = awaitEvent("shutdown", EXTENSION1_ID);
  let addon1 = await AddonManager.getAddonByID(EXTENSION1_ID);
  await addon1.disable();
  await disabledPromise;

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");

  let enabledPromise = awaitEvent("ready", EXTENSION1_ID);
  await addon1.enable();
  await enabledPromise;

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");
  await ext2.unload();

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");
  await ext1.unload();

  is(Services.search.currentEngine.name, defaultEngineName, `Default engine is ${defaultEngineName}`);
});

/* This tests that when two add-ons are installed that change default
 * search and the second one is disabled, the first one properly
 * gets the search. */
add_task(async function test_two_addons_with_second_disabled() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: EXTENSION1_ID,
        },
      },
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
      applications: {
        gecko: {
          id: EXTENSION2_ID,
        },
      },
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

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext2.startup();

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");

  let disabledPromise = awaitEvent("shutdown", EXTENSION2_ID);
  let addon2 = await AddonManager.getAddonByID(EXTENSION2_ID);
  await addon2.disable();
  await disabledPromise;

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");

  let enabledPromise = awaitEvent("ready", EXTENSION2_ID);
  await addon2.enable();
  await enabledPromise;

  is(Services.search.currentEngine.name, "Twitter", "Default engine is Twitter");
  await ext2.unload();

  is(Services.search.currentEngine.name, "DuckDuckGo", "Default engine is DuckDuckGo");
  await ext1.unload();

  is(Services.search.currentEngine.name, defaultEngineName, `Default engine is ${defaultEngineName}`);
});
