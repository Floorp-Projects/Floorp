/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* globals browser */

"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);

const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);

// Ignore rejection related to the storage.onChanged listener being removed while the extension context is being closed.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/Message manager disconnected/);

const { createAppInfo, promiseStartupManager } = AddonTestUtils;

const LEAVE_UUID_PREF = "extensions.webextensions.keepUuidOnUninstall";
const LEAVE_STORAGE_PREF = "extensions.webextensions.keepStorageOnUninstall";
const EXTENSION_STORAGE_ENABLED_PREF =
  "devtools.storage.extensionStorage.enabled";

AddonTestUtils.init(this);
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

ExtensionTestUtils.init(this);

// This storage actor is gated behind a pref, so make sure it is enabled first
Services.prefs.setBoolPref(EXTENSION_STORAGE_ENABLED_PREF, true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref(EXTENSION_STORAGE_ENABLED_PREF);
});

/**
 * Starts up Debugger server and connects a new Debugger client.
 *
 * @return {Promise} Resolves with a client object when the debugger has started up.
 */
async function startDebugger() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);
  await client.connect();
  return client;
}

/**
 * Set up the equivalent of an `about:debugging` toolbox for a given extension, minus
 * the toolbox.
 *
 * @param {String} id - The id for the extension to be targeted by the toolbox.
 * @return {Object} Resolves with the web extension actor front and target objects when
 * the debugger has been connected to the extension.
 */
async function setupExtensionDebugging(id) {
  const client = await startDebugger();
  const front = await client.mainRoot.getAddon({ id });
  // Starts a DevTools server in the extension child process.
  const target = await front.connect();
  return { front, target };
}

/**
 * Loads and starts up a test extension given the provided extension configuration.
 *
 * @param {Object} extConfig - The extension configuration object
 * @return {ExtensionWrapper} extension - Resolves with an extension object once the
 * extension has started up.
 */
async function startupExtension(extConfig) {
  const extension = ExtensionTestUtils.loadExtension(extConfig);

  await extension.startup();

  return extension;
}

/**
 * Initializes the extensionStorage actor for a target extension. This is effectively
 * what happens when the addon storage panel is opened in the browser.
 *
 * @param {String} - id, The addon id
 * @return {Object} - Resolves with the web extension actor target and extensionStorage
 * store objects when the panel has been opened.
 */
async function openAddonStoragePanel(id) {
  const { target } = await setupExtensionDebugging(id);

  const storageFront = await target.getFront("storage");
  const stores = await storageFront.listStores();
  const extensionStorage = stores.extensionStorage || null;

  return { target, extensionStorage };
}

/**
 * Builds the extension configuration object passed into ExtensionTestUtils.loadExtension
 *
 * @param {Object} options - Options, if any, to add to the configuration
 * @param {Function} options.background - A function comprising the test extension's
 * background script if provided
 * @param {Object} options.files - An object whose keys correspond to file names and
 * values map to the file contents
 * @param {Object} options.manifest - An object representing the extension's manifest
 * @return {Object} - The extension configuration object
 */
function getExtensionConfig(options = {}) {
  const { manifest, ...otherOptions } = options;
  const baseConfig = {
    manifest: {
      ...manifest,
      permissions: ["storage"],
    },
    useAddonManager: "temporary",
  };
  return {
    ...baseConfig,
    ...otherOptions,
  };
}

/**
 * An extension script that can be used in any extension context (e.g. as a background
 * script or as an extension page script loaded in a tab).
 */
async function extensionScriptWithMessageListener() {
  let fireOnChanged = false;
  browser.storage.onChanged.addListener(() => {
    if (fireOnChanged) {
      // Do not fire it again until explicitly requested again using the "storage-local-fireOnChanged" test message.
      fireOnChanged = false;
      browser.test.sendMessage("storage-local-onChanged");
    }
  });

  browser.test.onMessage.addListener(async (msg, ...args) => {
    let value = null;
    switch (msg) {
      case "storage-local-set":
        await browser.storage.local.set(args[0]);
        break;
      case "storage-local-get":
        value = (await browser.storage.local.get(args[0]))[args[0]];
        break;
      case "storage-local-remove":
        await browser.storage.local.remove(args[0]);
        break;
      case "storage-local-clear":
        await browser.storage.local.clear();
        break;
      case "storage-local-fireOnChanged": {
        // Allow the storage.onChanged listener to send a test event
        // message when onChanged is being fired.
        fireOnChanged = true;
        // Do not fire fireOnChanged:done.
        return;
      }
      default:
        browser.test.fail(`Unexpected test message: ${msg}`);
    }

    browser.test.sendMessage(`${msg}:done`, value);
  });
  browser.test.sendMessage("extension-origin", window.location.origin);
}

/**
 * Shared files for a test extension that has no background page but adds storage
 * items via a transient extension page in a tab
 */
const ext_no_bg = {
  files: {
    "extension_page_in_tab.html": `<!DOCTYPE html>
      <html>
        <head>
          <meta charset="utf-8">
        </head>
        <body>
          <h1>Extension Page in a Tab</h1>
          <script src="extension_page_in_tab.js"></script>
        </body>
      </html>`,
    "extension_page_in_tab.js": extensionScriptWithMessageListener,
  },
};

/**
 * Shutdown procedure common to all tasks.
 *
 * @param {Object} extension - The test extension
 * @param {Object} target - The web extension actor targeted by the DevTools client
 */
async function shutdown(extension, target) {
  if (target) {
    await target.destroy();
  }
  await extension.unload();
}

/**
 * Mocks the missing 'storage/permanent' directory needed by the "indexedDB"
 * storage actor's 'preListStores' method (called when 'listStores' is called). This
 * directory exists in a full browser i.e. mochitest.
 */
function createMissingIndexedDBDirs() {
  const dir = Services.dirsvc.get("ProfD", Ci.nsIFile).clone();
  dir.append("storage");
  if (!dir.exists()) {
    dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }
  dir.append("permanent");
  if (!dir.exists()) {
    dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }
  Assert.ok(
    dir.exists(),
    "Should have a 'storage/permanent' dir in the profile dir"
  );
}

add_task(async function setup() {
  await promiseStartupManager();
  createMissingIndexedDBDirs();
});

add_task(async function test_extension_store_exists() {
  const extension = await startupExtension(getExtensionConfig());

  const { target, extensionStorage } = await openAddonStoragePanel(
    extension.id
  );

  ok(extensionStorage, "Should have an extensionStorage store");

  await shutdown(extension, target);
});

add_task(
  {
    // This test currently fails if the extension runs in the main process
    // like in Thunderbird (see bug 1575183 comment #15 for details).
    skip_if: () => !WebExtensionPolicy.useRemoteWebExtensions,
  },
  async function test_extension_origin_matches_debugger_target() {
    async function background() {
      browser.test.sendMessage("extension-origin", window.location.origin);
    }

    const extension = await startupExtension(
      getExtensionConfig({ background })
    );

    const { target, extensionStorage } = await openAddonStoragePanel(
      extension.id
    );

    const { hosts } = extensionStorage;
    const expectedHost = await extension.awaitMessage("extension-origin");
    ok(
      expectedHost in hosts,
      "Should have the expected extension host in the extensionStorage store"
    );

    await shutdown(extension, target);
  }
);

/**
 * Test case: Background page modifies items while storage panel is open.
 * - Load extension with background page.
 * - Open the add-on debugger storage panel.
 * - With the panel still open, from the extension background page:
 *   - Bulk add storage items
 *   - Edit the values of some of the storage items
 *   - Remove some storage items
 *   - Clear all storage items
 * - For each modification, the storage data in the panel should match the
 *   changes made by the extension.
 */
add_task(async function test_panel_live_updates() {
  const extension = await startupExtension(
    getExtensionConfig({ background: extensionScriptWithMessageListener })
  );

  const { target, extensionStorage } = await openAddonStoragePanel(
    extension.id
  );

  const host = await extension.awaitMessage("extension-origin");

  let { data } = await extensionStorage.getStoreObjects(host);
  Assert.deepEqual(data, [], "Got the expected results on empty storage.local");

  info("Waiting for extension to bulk add 50 items to storage local");
  const bulkStorageItems = {};
  // limited by MAX_STORE_OBJECT_COUNT in devtools/server/actors/storage.js
  const numItems = 2;
  for (let i = 1; i <= numItems; i++) {
    bulkStorageItems[i] = i;
  }

  // fireOnChanged avoids the race condition where the extension
  // modifies storage then immediately tries to access storage before
  // the storage actor has finished updating.
  extension.sendMessage("storage-local-fireOnChanged");
  extension.sendMessage("storage-local-set", {
    ...bulkStorageItems,
    a: 123,
    b: [4, 5],
    c: { d: 678 },
    d: true,
    e: "hi",
    f: null,
  });
  await extension.awaitMessage("storage-local-set:done");
  await extension.awaitMessage("storage-local-onChanged");

  info(
    "Confirming items added by extension match items in extensionStorage store"
  );
  const bulkStorageObjects = [];
  for (const [name, value] of Object.entries(bulkStorageItems)) {
    bulkStorageObjects.push({
      area: "local",
      name,
      value: { str: String(value) },
    });
  }
  data = (await extensionStorage.getStoreObjects(host)).data;
  Assert.deepEqual(
    data,
    [
      ...bulkStorageObjects,
      { area: "local", name: "a", value: { str: "123" } },
      { area: "local", name: "b", value: { str: "[4,5]" } },
      { area: "local", name: "c", value: { str: '{"d":678}' } },
      { area: "local", name: "d", value: { str: "true" } },
      { area: "local", name: "e", value: { str: "hi" } },
      { area: "local", name: "f", value: { str: "null" } },
    ],
    "Got the expected results on populated storage.local"
  );

  info("Waiting for extension to edit a few storage item values");
  extension.sendMessage("storage-local-fireOnChanged");
  extension.sendMessage("storage-local-set", {
    a: ["c", "d"],
    b: 456,
    c: false,
  });
  await extension.awaitMessage("storage-local-set:done");
  await extension.awaitMessage("storage-local-onChanged");

  info(
    "Confirming items edited by extension match items in extensionStorage store"
  );
  data = (await extensionStorage.getStoreObjects(host)).data;
  Assert.deepEqual(
    data,
    [
      ...bulkStorageObjects,
      { area: "local", name: "a", value: { str: '["c","d"]' } },
      { area: "local", name: "b", value: { str: "456" } },
      { area: "local", name: "c", value: { str: "false" } },
      { area: "local", name: "d", value: { str: "true" } },
      { area: "local", name: "e", value: { str: "hi" } },
      { area: "local", name: "f", value: { str: "null" } },
    ],
    "Got the expected results on populated storage.local"
  );

  info("Waiting for extension to remove a few storage item values");
  extension.sendMessage("storage-local-fireOnChanged");
  extension.sendMessage("storage-local-remove", ["d", "e", "f"]);
  await extension.awaitMessage("storage-local-remove:done");
  await extension.awaitMessage("storage-local-onChanged");

  info(
    "Confirming items removed by extension were removed in extensionStorage store"
  );
  data = (await extensionStorage.getStoreObjects(host)).data;
  Assert.deepEqual(
    data,
    [
      ...bulkStorageObjects,
      { area: "local", name: "a", value: { str: '["c","d"]' } },
      { area: "local", name: "b", value: { str: "456" } },
      { area: "local", name: "c", value: { str: "false" } },
    ],
    "Got the expected results on populated storage.local"
  );

  info("Waiting for extension to remove all remaining storage items");
  extension.sendMessage("storage-local-fireOnChanged");
  extension.sendMessage("storage-local-clear");
  await extension.awaitMessage("storage-local-clear:done");
  await extension.awaitMessage("storage-local-onChanged");

  info("Confirming extensionStorage store was cleared");
  data = (await extensionStorage.getStoreObjects(host)).data;
  Assert.deepEqual(
    data,
    [],
    "Got the expected results on populated storage.local"
  );

  await shutdown(extension, target);
});

/**
 * Test case: No bg page. Transient page adds item before storage panel opened.
 * - Load extension with no background page.
 * - Open an extension page in a tab that adds a local storage item.
 * - With the extension page still open, open the add-on storage panel.
 * - The data in the storage panel should match the items added by the extension.
 */
add_task(
  async function test_panel_data_matches_extension_with_transient_page_open() {
    const extension = await startupExtension(
      getExtensionConfig({ files: ext_no_bg.files })
    );

    const url = extension.extension.baseURI.resolve(
      "extension_page_in_tab.html"
    );
    const contentPage = await ExtensionTestUtils.loadContentPage(url, {
      extension,
    });

    const host = await extension.awaitMessage("extension-origin");

    extension.sendMessage("storage-local-set", { a: 123 });
    await extension.awaitMessage("storage-local-set:done");

    const { target, extensionStorage } = await openAddonStoragePanel(
      extension.id
    );

    const { data } = await extensionStorage.getStoreObjects(host);
    Assert.deepEqual(
      data,
      [{ area: "local", name: "a", value: { str: "123" } }],
      "Got the expected results on populated storage.local"
    );

    await contentPage.close();
    await shutdown(extension, target);
  }
);

/**
 * Test case: No bg page. Transient page adds item then closes before storage panel opened.
 * - Load extension with no background page.
 * - Open an extension page in a tab that adds a local storage item.
 * - Close all extension pages.
 * - Open the add-on storage panel.
 * - The data in the storage panel should match the item added by the extension.
 */
add_task(async function test_panel_data_matches_extension_with_no_pages_open() {
  const extension = await startupExtension(
    getExtensionConfig({ files: ext_no_bg.files })
  );

  const url = extension.extension.baseURI.resolve("extension_page_in_tab.html");
  const contentPage = await ExtensionTestUtils.loadContentPage(url, {
    extension,
  });

  const host = await extension.awaitMessage("extension-origin");

  extension.sendMessage("storage-local-set", { a: 123 });
  await extension.awaitMessage("storage-local-set:done");

  await contentPage.close();

  const { target, extensionStorage } = await openAddonStoragePanel(
    extension.id
  );

  const { data } = await extensionStorage.getStoreObjects(host);
  Assert.deepEqual(
    data,
    [{ area: "local", name: "a", value: { str: "123" } }],
    "Got the expected results on populated storage.local"
  );

  await shutdown(extension, target);
});

/**
 * Test case: No bg page. Storage panel live updates when a transient page adds an item.
 * - Load extension with no background page.
 * - Open the add-on storage panel.
 * - With the storage panel still open, open an extension page in a new tab that adds an
 *   item.
 * - Assert:
 *   - The data in the storage panel should live update to match the item added by the
 *     extension.
 *   - If an extension page adds the same data again, the data in the storage panel should
 *     not change.
 */
add_task(
  async function test_panel_data_live_updates_for_extension_without_bg_page() {
    const extension = await startupExtension(
      getExtensionConfig({ files: ext_no_bg.files })
    );

    const { target, extensionStorage } = await openAddonStoragePanel(
      extension.id
    );

    const url = extension.extension.baseURI.resolve(
      "extension_page_in_tab.html"
    );
    const contentPage = await ExtensionTestUtils.loadContentPage(url, {
      extension,
    });

    const host = await extension.awaitMessage("extension-origin");

    let { data } = await extensionStorage.getStoreObjects(host);
    Assert.deepEqual(
      data,
      [],
      "Got the expected results on empty storage.local"
    );

    extension.sendMessage("storage-local-fireOnChanged");
    extension.sendMessage("storage-local-set", { a: 123 });
    await extension.awaitMessage("storage-local-set:done");
    await extension.awaitMessage("storage-local-onChanged");

    data = (await extensionStorage.getStoreObjects(host)).data;
    Assert.deepEqual(
      data,
      [{ area: "local", name: "a", value: { str: "123" } }],
      "Got the expected results on populated storage.local"
    );

    extension.sendMessage("storage-local-fireOnChanged");
    extension.sendMessage("storage-local-set", { a: 123 });
    await extension.awaitMessage("storage-local-set:done");
    await extension.awaitMessage("storage-local-onChanged");

    data = (await extensionStorage.getStoreObjects(host)).data;
    Assert.deepEqual(
      data,
      [{ area: "local", name: "a", value: { str: "123" } }],
      "The results are unchanged when an extension page adds duplicate items"
    );

    await contentPage.close();
    await shutdown(extension, target);
  }
);

/**
 * Test case: Storage panel shows extension storage data added prior to extension startup
 * - Load extension that adds a storage item
 * - Uninstall the extension
 * - Reinstall the extension
 * - Open the add-on storage panel.
 * - The data in the storage panel should match the data added the first time the extension
 *   was installed
 * Related test case: Storage panel shows extension storage data when an extension that has
 * already migrated to the IndexedDB storage backend prior to extension startup adds
 * another storage item.
 * - (Building from previous steps)
 * - The reinstalled extension adds a storage item
 * - The data in the storage panel should live update with both items: the item added from
 *   the first and the item added from the reinstall.
 */
add_task(
  async function test_panel_data_matches_data_added_prior_to_ext_startup() {
    // The pref to leave the addonid->uuid mapping around after uninstall so that we can
    // re-attach to the same storage
    Services.prefs.setBoolPref(LEAVE_UUID_PREF, true);

    // The pref to prevent cleaning up storage on uninstall
    Services.prefs.setBoolPref(LEAVE_STORAGE_PREF, true);

    let extension = await startupExtension(
      getExtensionConfig({ background: extensionScriptWithMessageListener })
    );

    const host = await extension.awaitMessage("extension-origin");

    extension.sendMessage("storage-local-set", { a: 123 });
    await extension.awaitMessage("storage-local-set:done");

    await shutdown(extension);

    // Reinstall the same extension
    extension = await startupExtension(
      getExtensionConfig({ background: extensionScriptWithMessageListener })
    );

    await extension.awaitMessage("extension-origin");

    const { target, extensionStorage } = await openAddonStoragePanel(
      extension.id
    );

    let { data } = await extensionStorage.getStoreObjects(host);
    Assert.deepEqual(
      data,
      [{ area: "local", name: "a", value: { str: "123" } }],
      "Got the expected results on populated storage.local"
    );

    // Related test case
    extension.sendMessage("storage-local-fireOnChanged");
    extension.sendMessage("storage-local-set", { b: 456 });
    await extension.awaitMessage("storage-local-set:done");
    await extension.awaitMessage("storage-local-onChanged");

    data = (await extensionStorage.getStoreObjects(host)).data;
    Assert.deepEqual(
      data,
      [
        { area: "local", name: "a", value: { str: "123" } },
        { area: "local", name: "b", value: { str: "456" } },
      ],
      "Got the expected results on populated storage.local"
    );

    Services.prefs.setBoolPref(LEAVE_STORAGE_PREF, false);
    Services.prefs.setBoolPref(LEAVE_UUID_PREF, false);

    await shutdown(extension, target);
  }
);

add_task(
  function cleanup_for_test_panel_data_matches_data_added_prior_to_ext_startup() {
    Services.prefs.clearUserPref(LEAVE_UUID_PREF);
    Services.prefs.clearUserPref(LEAVE_STORAGE_PREF);
  }
);

/**
 * Test case: Bg page adds an item to storage. With storage panel open, reload extension.
 * - Load extension with background page that adds a storage item on message.
 * - Open the add-on storage panel.
 * - With the storage panel still open, reload the extension.
 * - The data in the storage panel should match the item added prior to reloading.
 */
add_task(async function test_panel_live_reload() {
  const EXTENSION_ID = "test_panel_live_reload@xpcshell.mozilla.org";
  let manifest = {
    version: "1.0",
    applications: {
      gecko: {
        id: EXTENSION_ID,
      },
    },
  };

  info("Loading extension version 1.0");
  const extension = await startupExtension(
    getExtensionConfig({
      manifest,
      background: extensionScriptWithMessageListener,
    })
  );

  info("Waiting for message from test extension");
  const host = await extension.awaitMessage("extension-origin");

  info("Adding storage item");
  extension.sendMessage("storage-local-set", { a: 123 });
  await extension.awaitMessage("storage-local-set:done");

  const { target, extensionStorage } = await openAddonStoragePanel(
    extension.id
  );

  manifest = {
    ...manifest,
    version: "2.0",
  };
  // "Reload" is most similar to an upgrade, as e.g. storage data is preserved
  info("Update to version 2.0");
  await extension.upgrade(
    getExtensionConfig({
      manifest,
      background: extensionScriptWithMessageListener,
    })
  );

  await extension.awaitMessage("extension-origin");

  const { data } = await extensionStorage.getStoreObjects(host);
  Assert.deepEqual(
    data,
    [{ area: "local", name: "a", value: { str: "123" } }],
    "Got the expected results on populated storage.local"
  );

  await shutdown(extension, target);
});

/**
 * Test case: Transient page adds an item to storage. With storage panel open,
 * reload extension.
 * - Load extension with no background page.
 * - Open transient page that adds a storage item on message.
 * - Open the add-on storage panel.
 * - With the storage panel still open, reload the extension.
 * - The data in the storage panel should match the item added prior to reloading.
 */
add_task(async function test_panel_live_reload_for_extension_without_bg_page() {
  const EXTENSION_ID = "test_local_storage_live_reload@xpcshell.mozilla.org";
  let manifest = {
    version: "1.0",
    applications: {
      gecko: {
        id: EXTENSION_ID,
      },
    },
  };

  info("Loading and starting extension version 1.0");
  const extension = await startupExtension(
    getExtensionConfig({
      manifest,
      files: ext_no_bg.files,
    })
  );

  info("Opening extension page in a tab");
  const url = extension.extension.baseURI.resolve("extension_page_in_tab.html");
  const contentPage = await ExtensionTestUtils.loadContentPage(url, {
    extension,
  });

  const host = await extension.awaitMessage("extension-origin");

  info("Waiting for extension page in a tab to add storage item");
  extension.sendMessage("storage-local-set", { a: 123 });
  await extension.awaitMessage("storage-local-set:done");

  await contentPage.close();

  info("Opening storage panel");
  const { target, extensionStorage } = await openAddonStoragePanel(
    extension.id
  );

  manifest = {
    ...manifest,
    version: "2.0",
  };
  // "Reload" is most similar to an upgrade, as e.g. storage data is preserved
  info("Updating extension to version 2.0");
  await extension.upgrade(
    getExtensionConfig({
      manifest,
      files: ext_no_bg.files,
    })
  );

  const { data } = await extensionStorage.getStoreObjects(host);
  Assert.deepEqual(
    data,
    [{ area: "local", name: "a", value: { str: "123" } }],
    "Got the expected results on populated storage.local"
  );

  await shutdown(extension, target);
});

/**
 * Test case: Bg page auto adds item(s). With storage panel open, reload extension.
 * - Load extension with background page that automatically adds a storage item on startup.
 * - Open the add-on storage panel.
 * - With the storage panel still open, reload the extension.
 * - The data in the storage panel should match the item(s) added by the reloaded
 *   extension.
 */
add_task(
  async function test_panel_live_reload_when_extension_auto_adds_items() {
    async function background() {
      await browser.storage.local.set({ a: { b: 123 }, c: { d: 456 } });
      browser.test.sendMessage("extension-origin", window.location.origin);
    }
    const EXTENSION_ID = "test_local_storage_live_reload@xpcshell.mozilla.org";
    let manifest = {
      version: "1.0",
      applications: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    };

    info("Loading and starting extension version 1.0");
    const extension = await startupExtension(
      getExtensionConfig({ manifest, background })
    );

    info("Waiting for message from test extension");
    const host = await extension.awaitMessage("extension-origin");

    info("Opening storage panel");
    const { target, extensionStorage } = await openAddonStoragePanel(
      extension.id
    );

    manifest = {
      ...manifest,
      version: "2.0",
    };
    // "Reload" is most similar to an upgrade, as e.g. storage data is preserved
    info("Update to version 2.0");
    await extension.upgrade(
      getExtensionConfig({
        manifest,
        background,
      })
    );

    await extension.awaitMessage("extension-origin");

    const { data } = await extensionStorage.getStoreObjects(host);
    Assert.deepEqual(
      data,
      [
        { area: "local", name: "a", value: { str: '{"b":123}' } },
        { area: "local", name: "c", value: { str: '{"d":456}' } },
      ],
      "Got the expected results on populated storage.local"
    );

    await shutdown(extension, target);
  }
);

/*
 * This task should be last, as it sets a pref to disable the extensionStorage
 * storage actor. Since this pref is set at the beginning of the file, it
 * already will be cleared via registerCleanupFunction when the test finishes.
 */
add_task(
  {
    // This test fails if the extension runs in the main process
    // like in Thunderbird (see bug 1575183 comment #15 for details).
    skip_if: () => !WebExtensionPolicy.useRemoteWebExtensions,
  },
  async function test_extensionStorage_store_disabled_on_pref() {
    Services.prefs.setBoolPref(EXTENSION_STORAGE_ENABLED_PREF, false);

    const extension = await startupExtension(getExtensionConfig());

    const { target, extensionStorage } = await openAddonStoragePanel(
      extension.id
    );

    ok(
      extensionStorage === null,
      "Should not have an extensionStorage store when pref disabled"
    );

    await shutdown(extension, target);
  }
);
