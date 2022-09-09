/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* globals browser */

"use strict";

/**
 * Test helpers shared by the devtools server xpcshell tests related to webextensions.
 */

const { FileUtils } = require("resource://gre/modules/FileUtils.jsm");
const { Ci } = require("chrome");
const {
  ExtensionTestUtils,
} = require("resource://testing-common/ExtensionXPCShellUtils.jsm");

const { DevToolsServer } = require("devtools/server/devtools-server");
const { DevToolsClient } = require("devtools/client/devtools-client");

/**
 * Starts up DevTools server and connects a new DevTools client.
 *
 * @return {Promise} Resolves with a client object when the debugger has started up.
 */
async function startDebugger() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  await client.connect();
  return client;
}
exports.startDebugger = startDebugger;

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
  const target = await front.getTarget();
  return { front, target };
}
exports.setupExtensionDebugging = setupExtensionDebugging;

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
exports.startupExtension = startupExtension;

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

  return { target, extensionStorage, storageFront };
}
exports.openAddonStoragePanel = openAddonStoragePanel;

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
exports.getExtensionConfig = getExtensionConfig;

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
exports.ext_no_bg = ext_no_bg;

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
    let item = null;
    switch (msg) {
      case "storage-local-set":
        await browser.storage.local.set(args[0]);
        break;
      case "storage-local-get":
        item = await browser.storage.local.get(args[0]);
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

    browser.test.sendMessage(`${msg}:done`, item);
  });
  // window is available in background scripts
  // eslint-disable-next-line no-undef
  browser.test.sendMessage("extension-origin", window.location.origin);
}
exports.extensionScriptWithMessageListener = extensionScriptWithMessageListener;

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
exports.shutdown = shutdown;

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

  return dir;
}
exports.createMissingIndexedDBDirs = createMissingIndexedDBDirs;
