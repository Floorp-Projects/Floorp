/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);

const EXTENSION1_ID = "extension1@mozilla.com";
const EXTENSION2_ID = "extension2@mozilla.com";
const DEFAULT_SEARCH_STORE_TYPE = "default_search";
const DEFAULT_SEARCH_SETTING_NAME = "defaultSearch";

AddonTestUtils.initMochitest(this);
SearchTestUtils.init(this);

const DEFAULT_ENGINE_NAME = "basic";
const ALTERNATE_ENGINE_NAME = "Simple Engine";
const ALTERNATE2_ENGINE_NAME = "another";

async function restoreDefaultEngine() {
  let engine = Services.search.getEngineByName(DEFAULT_ENGINE_NAME);
  await Services.search.setDefault(engine);
}

add_setup(async function() {
  let searchExtensions = getChromeDir(getResolvedURI(gTestPath));
  searchExtensions.append("search-engines");

  await SearchTestUtils.useMochitestEngines(searchExtensions);

  SearchTestUtils.useMockIdleService();
  let response = await fetch(`resource://search-extensions/engines.json`);
  let json = await response.json();
  await SearchTestUtils.updateRemoteSettingsConfig(json.data);

  registerCleanupFunction(async () => {
    let settingsWritten = SearchTestUtils.promiseSearchNotification(
      "write-settings-to-disk-complete"
    );
    await SearchTestUtils.updateRemoteSettingsConfig();
    await settingsWritten;
  });
});

/* This tests setting a default engine. */
add_task(async function test_extension_setting_default_engine() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE_ENGINE_NAME,
    `Default engine is ${ALTERNATE_ENGINE_NAME}`
  );

  await ext1.unload();

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    `Default engine is ${DEFAULT_ENGINE_NAME}`
  );
});

/* This tests what happens when the engine you're setting it to is hidden. */
add_task(async function test_extension_setting_default_engine_hidden() {
  let engine = Services.search.getEngineByName(ALTERNATE_ENGINE_NAME);
  engine.hidden = true;

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    "Default engine should have remained as the default"
  );
  is(
    ExtensionSettingsStore.getSetting("default_search", "defaultSearch"),
    null,
    "The extension should not have been recorded as having set the default search"
  );

  await ext1.unload();

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    `Default engine is ${DEFAULT_ENGINE_NAME}`
  );
  engine.hidden = false;
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
        applications: {
          gecko: {
            id: EXTENSION1_ID,
          },
        },
        chrome_settings_overrides: {
          search_provider: {
            name: NAME,
            search_url: "https://example.com/?q={searchTerms}",
            is_default: true,
          },
        },
      },
      useAddonManager: "temporary",
    });

    let [panel] = await Promise.all([
      promisePopupNotificationShown("addon-webext-defaultsearch", win),
      extension.startup(),
    ]);

    isnot(
      panel,
      null,
      "Doorhanger was displayed for non-built-in default engine"
    );

    return { panel, extension };
  }

  // First time around, don't accept the default engine.
  let { panel, extension } = await startExtension();
  panel.secondaryButton.click();

  await TestUtils.topicObserved("webextension-defaultsearch-prompt-response");

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    "Default engine was not changed after rejecting prompt"
  );

  await extension.unload();

  // Do it again, this time accept the prompt.
  ({ panel, extension } = await startExtension());

  panel.button.click();
  await TestUtils.topicObserved("webextension-defaultsearch-prompt-response");

  is(
    (await Services.search.getDefault()).name,
    NAME,
    "Default engine was changed after accepting prompt"
  );

  // Do this twice to make sure we're definitely handling disable/enable
  // correctly.  Disabling and enabling the addon here like this also
  // replicates the behavior when an addon is added then removed in the
  // blocklist.
  let disabledPromise = awaitEvent("shutdown", EXTENSION1_ID);
  let addon = await AddonManager.getAddonByID(EXTENSION1_ID);
  await addon.disable();
  await disabledPromise;

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    `Default engine is ${DEFAULT_ENGINE_NAME} after disabling`
  );

  let opened = promisePopupNotificationShown(
    "addon-webext-defaultsearch",
    window
  );
  await addon.enable();
  panel = await opened;
  panel.button.click();
  await TestUtils.topicObserved("webextension-defaultsearch-prompt-response");

  is(
    (await Services.search.getDefault()).name,
    NAME,
    `Default engine is ${NAME} after enabling`
  );

  await extension.unload();

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    "Default engine is reverted after uninstalling extension."
  );

  // One more time, this time close the window where the prompt
  // appears instead of explicitly accepting or denying it.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank");

  ({ extension } = await startExtension(win));

  await BrowserTestUtils.closeWindow(win);

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    "Default engine is unchanged when prompt is dismissed"
  );

  await extension.unload();
});

/* This tests that uninstalling add-ons maintains the proper
 * search default. */
add_task(async function test_extension_setting_multiple_default_engine() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  let ext2 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE2_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE_ENGINE_NAME,
    `Default engine is ${ALTERNATE_ENGINE_NAME}`
  );

  await ext2.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext2);

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );

  await ext2.unload();

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE_ENGINE_NAME,
    `Default engine is ${ALTERNATE_ENGINE_NAME}`
  );

  await ext1.unload();

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    `Default engine is ${DEFAULT_ENGINE_NAME}`
  );
});

/* This tests that uninstalling add-ons in reverse order maintains the proper
 * search default. */
add_task(
  async function test_extension_setting_multiple_default_engine_reversed() {
    let ext1 = ExtensionTestUtils.loadExtension({
      manifest: {
        chrome_settings_overrides: {
          search_provider: {
            name: ALTERNATE_ENGINE_NAME,
            search_url: "https://example.com/?q={searchTerms}",
            is_default: true,
          },
        },
      },
      useAddonManager: "temporary",
    });

    let ext2 = ExtensionTestUtils.loadExtension({
      manifest: {
        chrome_settings_overrides: {
          search_provider: {
            name: ALTERNATE2_ENGINE_NAME,
            search_url: "https://example.com/?q={searchTerms}",
            is_default: true,
          },
        },
      },
      useAddonManager: "temporary",
    });

    await ext1.startup();
    await AddonTestUtils.waitForSearchProviderStartup(ext1);

    is(
      (await Services.search.getDefault()).name,
      ALTERNATE_ENGINE_NAME,
      `Default engine is ${ALTERNATE_ENGINE_NAME}`
    );

    await ext2.startup();
    await AddonTestUtils.waitForSearchProviderStartup(ext2);

    is(
      (await Services.search.getDefault()).name,
      ALTERNATE2_ENGINE_NAME,
      `Default engine is ${ALTERNATE2_ENGINE_NAME}`
    );

    await ext1.unload();

    is(
      (await Services.search.getDefault()).name,
      ALTERNATE2_ENGINE_NAME,
      `Default engine is ${ALTERNATE2_ENGINE_NAME}`
    );

    await ext2.unload();

    is(
      (await Services.search.getDefault()).name,
      DEFAULT_ENGINE_NAME,
      `Default engine is ${DEFAULT_ENGINE_NAME}`
    );
  }
);

/* This tests that when the user changes the search engine and the add-on
 * is unistalled, search stays with the user's choice. */
add_task(async function test_user_changing_default_engine() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE_ENGINE_NAME,
    `Default engine is ${ALTERNATE_ENGINE_NAME}`
  );

  let engine = Services.search.getEngineByName(ALTERNATE2_ENGINE_NAME);
  await Services.search.setDefault(engine);
  // This simulates the preferences UI when the setting is changed.
  ExtensionSettingsStore.select(
    ExtensionSettingsStore.SETTING_USER_SET,
    DEFAULT_SEARCH_STORE_TYPE,
    DEFAULT_SEARCH_SETTING_NAME
  );

  await ext1.unload();

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );
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
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE_ENGINE_NAME,
    `Default engine is ${ALTERNATE_ENGINE_NAME}`
  );

  let engine = Services.search.getEngineByName(ALTERNATE2_ENGINE_NAME);
  await Services.search.setDefault(engine);
  // This simulates the preferences UI when the setting is changed.
  ExtensionSettingsStore.select(
    ExtensionSettingsStore.SETTING_USER_SET,
    DEFAULT_SEARCH_STORE_TYPE,
    DEFAULT_SEARCH_SETTING_NAME
  );

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );

  let disabledPromise = awaitEvent("shutdown", EXTENSION1_ID);
  let addon = await AddonManager.getAddonByID(EXTENSION1_ID);
  await addon.disable();
  await disabledPromise;

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );

  let processedPromise = awaitEvent("searchEngineProcessed", EXTENSION1_ID);
  await addon.enable();
  await processedPromise;

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );
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
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
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
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE2_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE_ENGINE_NAME,
    `Default engine is ${ALTERNATE_ENGINE_NAME}`
  );

  let disabledPromise = awaitEvent("shutdown", EXTENSION1_ID);
  let addon1 = await AddonManager.getAddonByID(EXTENSION1_ID);
  await addon1.disable();
  await disabledPromise;

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    `Default engine is ${DEFAULT_ENGINE_NAME}`
  );

  await ext2.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext2);

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );

  let enabledPromise = awaitEvent("ready", EXTENSION1_ID);
  await addon1.enable();
  await enabledPromise;

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );
  await ext2.unload();
  await ext1.unload();

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    `Default engine is ${DEFAULT_ENGINE_NAME}`
  );
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
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
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
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE2_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE_ENGINE_NAME,
    `Default engine is ${ALTERNATE_ENGINE_NAME}`
  );

  await ext2.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext2);

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );

  let disabledPromise = awaitEvent("shutdown", EXTENSION1_ID);
  let addon1 = await AddonManager.getAddonByID(EXTENSION1_ID);
  await addon1.disable();
  await disabledPromise;

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );

  let enabledPromise = awaitEvent("ready", EXTENSION1_ID);
  await addon1.enable();
  await enabledPromise;

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );
  await ext2.unload();
  await ext1.unload();

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    `Default engine is ${DEFAULT_ENGINE_NAME}`
  );
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
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
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
      chrome_settings_overrides: {
        search_provider: {
          name: ALTERNATE2_ENGINE_NAME,
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext1);

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE_ENGINE_NAME,
    `Default engine is ${ALTERNATE_ENGINE_NAME}`
  );

  await ext2.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext2);

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );

  let disabledPromise = awaitEvent("shutdown", EXTENSION2_ID);
  let addon2 = await AddonManager.getAddonByID(EXTENSION2_ID);
  await addon2.disable();
  await disabledPromise;

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE_ENGINE_NAME,
    `Default engine is ${ALTERNATE_ENGINE_NAME}`
  );

  let defaultPromise = SearchTestUtils.promiseSearchNotification(
    "engine-default",
    "browser-search-engine-modified"
  );
  // No prompt, because this is switching to an app-provided engine.
  await addon2.enable();
  await defaultPromise;

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE2_ENGINE_NAME,
    `Default engine is ${ALTERNATE2_ENGINE_NAME}`
  );
  await ext2.unload();

  is(
    (await Services.search.getDefault()).name,
    ALTERNATE_ENGINE_NAME,
    `Default engine is ${ALTERNATE_ENGINE_NAME}`
  );
  await ext1.unload();

  is(
    (await Services.search.getDefault()).name,
    DEFAULT_ENGINE_NAME,
    `Default engine is ${DEFAULT_ENGINE_NAME}`
  );
});
