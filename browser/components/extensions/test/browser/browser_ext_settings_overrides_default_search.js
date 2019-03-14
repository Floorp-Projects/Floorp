/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");

const EXTENSION1_ID = "extension1@mozilla.com";
const EXTENSION2_ID = "extension2@mozilla.com";

var defaultEngineName;

async function restoreDefaultEngine() {
  let engine = Services.search.getEngineByName(defaultEngineName);
  await Services.search.setDefault(engine);
}

add_task(async function setup() {
  defaultEngineName = (await Services.search.getDefault()).name;
  registerCleanupFunction(restoreDefaultEngine);
});

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

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext1.unload();

  is((await Services.search.getDefault()).name, defaultEngineName, `Default engine is ${defaultEngineName}`);
});

// Test the popup displayed when trying to add a non-built-in default
// search engine.
add_task(async function test_extension_setting_default_engine_external() {
  const NAME = "Example Engine";

  // Load an extension that tries to set the default engine,
  // and wait for the ensuing prompt.
  async function startExtension(win = window) {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        "chrome_settings_overrides": {
          "search_provider": {
            "name": NAME,
            "search_url": "https://example.com/?q={searchTerms}",
            "is_default": true,
          },
        },
      },
      useAddonManager: "temporary",
    });

    let [panel] = await Promise.all([
      promisePopupNotificationShown("addon-webext-defaultsearch", win),
      extension.startup(),
    ]);

    isnot(panel, null, "Doorhanger was displayed for non-built-in default engine");

    return {panel, extension};
  }

  // First time around, don't accept the default engine.
  let {panel, extension} = await startExtension();
  panel.secondaryButton.click();

  // There is no explicit event we can wait for to know when the click
  // callback has been fully processed.  One spin through the Promise
  // microtask queue should be enough.  If this wait isn't long enough,
  // the test below where we accept the prompt will fail.
  await Promise.resolve();

  is((await Services.search.getDefault()).name, defaultEngineName,
     "Default engine was not changed after rejecting prompt");

  await extension.unload();

  // Do it again, this time accept the prompt.
  ({panel, extension} = await startExtension());

  panel.button.click();
  await Promise.resolve();

  is((await Services.search.getDefault()).name, NAME,
     "Default engine was changed after accepting prompt");

  await extension.unload();

  is((await Services.search.getDefault()).name, defaultEngineName,
     "Default engine is reverted after uninstalling extension.");

  // One more time, this time close the window where the prompt
  // appears instead of explicitly accepting or denying it.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank");

  ({extension} = await startExtension(win));

  await BrowserTestUtils.closeWindow(win);

  is((await Services.search.getDefault()).name, defaultEngineName,
     "Default engine is unchanged when prompt is dismissed");

  await extension.unload();
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

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext2.startup();

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");

  await ext2.unload();

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext1.unload();

  is((await Services.search.getDefault()).name, defaultEngineName, `Default engine is ${defaultEngineName}`);
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

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext2.startup();

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");

  await ext1.unload();

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");

  await ext2.unload();

  is((await Services.search.getDefault()).name, defaultEngineName, `Default engine is ${defaultEngineName}`);
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

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");

  let engine = Services.search.getEngineByName("Twitter");
  await Services.search.setDefault(engine);

  await ext1.unload();

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");
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

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");

  let engine = Services.search.getEngineByName("Twitter");
  await Services.search.setDefault(engine);

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");

  let disabledPromise = awaitEvent("shutdown", EXTENSION1_ID);
  let addon = await AddonManager.getAddonByID(EXTENSION1_ID);
  await addon.disable();
  await disabledPromise;

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");

  let enabledPromise = awaitEvent("ready", EXTENSION1_ID);
  await addon.enable();
  await enabledPromise;

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");
  await ext1.unload();
  await restoreDefaultEngine();
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

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");

  let disabledPromise = awaitEvent("shutdown", EXTENSION1_ID);
  let addon1 = await AddonManager.getAddonByID(EXTENSION1_ID);
  await addon1.disable();
  await disabledPromise;

  is((await Services.search.getDefault()).name, defaultEngineName, `Default engine is ${defaultEngineName}`);

  await ext2.startup();

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");

  let enabledPromise = awaitEvent("ready", EXTENSION1_ID);
  await addon1.enable();
  await enabledPromise;

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");
  await ext2.unload();

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");
  await ext1.unload();

  is((await Services.search.getDefault()).name, defaultEngineName, `Default engine is ${defaultEngineName}`);
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

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext2.startup();

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");

  let disabledPromise = awaitEvent("shutdown", EXTENSION1_ID);
  let addon1 = await AddonManager.getAddonByID(EXTENSION1_ID);
  await addon1.disable();
  await disabledPromise;

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");

  let enabledPromise = awaitEvent("ready", EXTENSION1_ID);
  await addon1.enable();
  await enabledPromise;

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");
  await ext2.unload();

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");
  await ext1.unload();

  is((await Services.search.getDefault()).name, defaultEngineName, `Default engine is ${defaultEngineName}`);
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

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");

  await ext2.startup();

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");

  let disabledPromise = awaitEvent("shutdown", EXTENSION2_ID);
  let addon2 = await AddonManager.getAddonByID(EXTENSION2_ID);
  await addon2.disable();
  await disabledPromise;

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");

  let enabledPromise = awaitEvent("ready", EXTENSION2_ID);
  await addon2.enable();
  await enabledPromise;

  is((await Services.search.getDefault()).name, "Twitter", "Default engine is Twitter");
  await ext2.unload();

  is((await Services.search.getDefault()).name, "DuckDuckGo", "Default engine is DuckDuckGo");
  await ext1.unload();

  is((await Services.search.getDefault()).name, defaultEngineName, `Default engine is ${defaultEngineName}`);
});
