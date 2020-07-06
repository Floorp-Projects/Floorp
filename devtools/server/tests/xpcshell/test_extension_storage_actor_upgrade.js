/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Note: this test used to be in test_extension_storage_actor.js, but seems to
 * fail frequently as soon as we start auto-attaching targets.
 * See Bug 1618059.
 */

const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);

const {
  createMissingIndexedDBDirs,
  extensionScriptWithMessageListener,
  getExtensionConfig,
  openAddonStoragePanel,
  shutdown,
  startupExtension,
} = require("resource://test/helper_test_extension_storage_actor.js");

// Ignore rejection related to the storage.onChanged listener being removed while the extension context is being closed.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Message manager disconnected/
);

const { createAppInfo, promiseStartupManager } = AddonTestUtils;

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

add_task(async function setup() {
  await promiseStartupManager();
  const dir = createMissingIndexedDBDirs();

  Assert.ok(
    dir.exists(),
    "Should have a 'storage/permanent' dir in the profile dir"
  );
});

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

  // When the webextension target is attached, we need to wait until the storage
  // is populated with the proper data again.
  // XXX: This seems to be a race which has a much higher frequency when the
  // target is attached, but the relationship between the two is unclear.
  await asyncWaitUntil(async () => {
    const { data } = await extensionStorage.getStoreObjects(host);
    return data?.length > 0;
  });

  const { data } = await extensionStorage.getStoreObjects(host);
  Assert.deepEqual(
    data,
    [
      {
        area: "local",
        name: "a",
        value: { str: "123" },
        isValueEditable: true,
      },
    ],
    "Got the expected results on populated storage.local"
  );

  await shutdown(extension, target);
});

/**
 * Duplicated from shared-head.js
 */
async function asyncWaitUntil(predicate, interval = 100) {
  let success = await predicate();
  const { setTimeout } = require("resource://gre/modules/Timer.jsm");
  while (!success) {
    // Wait for X milliseconds.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, interval));
    // Test the predicate again.
    success = await predicate();
  }
}
