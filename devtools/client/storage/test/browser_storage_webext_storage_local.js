/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* globals browser BigInt */

"use strict";

loader.lazyRequireGetter(
  this,
  "DevToolsServer",
  "devtools/server/devtools-server",
  true
);
loader.lazyRequireGetter(
  this,
  "DevToolsClient",
  "devtools/client/devtools-client",
  true
);

const { Toolbox } = require("devtools/client/framework/toolbox");

/**
 * Initialize and connect a DevToolsServer and DevToolsClient. Note: This test
 * does not use TabDescriptorFactory, so it has to set up the DevToolsServer and
 * DevToolsClient on its own.
 * @return {Promise} Resolves with an instance of the DevToolsClient class
 */
async function setupLocalDevToolsServerAndClient() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  const client = new DevToolsClient(DevToolsServer.connectPipe());
  await client.connect();
  return client;
}

/**
 * Set up and optionally open the `about:debugging` toolbox for a given extension.
 *
 * @param {String} id
 *        The id for the extension to be targeted by the toolbox.
 * @return {Promise} Resolves with a web extension actor target object and the
 *         toolbox and storage objects when the toolbox has been setup
 */
async function setupExtensionDebuggingToolbox(id) {
  const client = await setupLocalDevToolsServerAndClient();
  const descriptor = await client.mainRoot.getAddon({ id });

  const { toolbox, storage } = await openStoragePanel({
    descriptor,
    hostType: Toolbox.HostType.WINDOW,
  });

  const target = toolbox.target;
  target.shouldCloseClient = true;

  return { target, toolbox, storage };
}

add_task(async function set_enable_extensionStorage_pref() {
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.storage.extensionStorage.enabled", true]],
  });
});

/**
 * Since storage item values are represented in the client as strings in textboxes, not all
 * JavaScript object types supported by the WE storage local API and its IndexedDB backend
 * can be successfully stringified for display in the table much less parsed correctly when
 * the user tries to edit a value in the panel. This test is expected to change over time
 * as more and more value types are supported.
 */
add_task(
  async function test_extension_toolbox_only_supported_values_editable() {
    async function background() {
      browser.test.onMessage.addListener(async (msg, ...args) => {
        switch (msg) {
          case "storage-local-set":
            await browser.storage.local.set(args[0]);
            break;
          case "storage-local-get": {
            const items = await browser.storage.local.get(args[0]);
            for (const [key, val] of Object.entries(items)) {
              browser.test.assertTrue(
                val === args[1],
                `New value ${val} is set for key ${key}.`
              );
            }
            break;
          }
          case "storage-local-fireOnChanged": {
            const listener = () => {
              browser.storage.onChanged.removeListener(listener);
              browser.test.sendMessage("storage-local-onChanged");
            };
            browser.storage.onChanged.addListener(listener);
            // Call an API method implemented in the parent process
            // to ensure that the listener has been registered
            // in the main process before the test proceeds.
            await browser.runtime.getPlatformInfo();
            break;
          }
          default:
            browser.test.fail(`Unexpected test message: ${msg}`);
        }

        browser.test.sendMessage(`${msg}:done`);
      });
      browser.test.sendMessage("extension-origin", window.location.origin);
    }
    const extension = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["storage"],
      },
      background,
      useAddonManager: "temporary",
    });

    await extension.startup();

    const host = await extension.awaitMessage("extension-origin");

    const itemsSupported = {
      arr: [1, 2],
      bool: true,
      null: null,
      num: 4,
      obj: { a: 123 },
      str: "hi",
      // Nested objects or arrays at most 2 levels deep should be editable
      nestedArr: [
        {
          a: "b",
        },
        "c",
      ],
      nestedObj: {
        a: [1, 2],
        b: 3,
      },
    };

    const itemsUnsupported = {
      arrBuffer: new ArrayBuffer(8),
      bigint: BigInt(1),
      blob: new Blob(
        [
          JSON.stringify(
            {
              hello: "world",
            },
            null,
            2
          ),
        ],
        {
          type: "application/json",
        }
      ),
      date: new Date(0),
      map: new Map().set("a", "b"),
      regexp: /regexp/,
      set: new Set().add(1).add("a"),
      undef: undefined,
      // Arrays and object literals with non-JSONifiable values should not be editable
      arrWithMap: [1, new Map().set("a", 1)],
      objWithArrayBuffer: { a: new ArrayBuffer(8) },
      // Nested objects or arrays more than 2 levels deep should not be editable
      deepNestedArr: [[{ a: "b" }, 3], 4],
      deepNestedObj: {
        a: {
          b: [1, 2],
        },
      },
    };

    info("Add storage items from the extension");
    const allItems = { ...itemsSupported, ...itemsUnsupported };
    extension.sendMessage("storage-local-fireOnChanged");
    await extension.awaitMessage("storage-local-fireOnChanged:done");
    extension.sendMessage("storage-local-set", allItems);
    info(
      "Wait for the extension to add storage items and receive the 'onChanged' event"
    );
    await extension.awaitMessage("storage-local-set:done");
    await extension.awaitMessage("storage-local-onChanged");

    info("Open the addon toolbox storage panel");
    const { target, toolbox } = await setupExtensionDebuggingToolbox(
      extension.id
    );

    await selectTreeItem(["extensionStorage", host]);

    info("Verify that value types supported by the storage actor are editable");
    let validate = true;
    const newValue = "anotherValue";
    const supportedIds = Object.keys(itemsSupported);
    for (const id of supportedIds) {
      startCellEdit(id, "value", newValue);
      await editCell(id, "value", newValue, validate);
    }

    info("Verify that associated values have been changed in the extension");
    extension.sendMessage(
      "storage-local-get",
      Object.keys(itemsSupported),
      newValue
    );
    await extension.awaitMessage("storage-local-get:done");

    info(
      "Verify that value types not supported by the storage actor are uneditable"
    );
    const expectedValStrings = {
      arrBuffer: "{}",
      bigint: "1n",
      blob: "{}",
      date: "1970-01-01T00:00:00.000Z",
      map: "{}",
      regexp: "{}",
      set: "{}",
      undef: "undefined",
      arrWithMap: "[1,{}]",
      objWithArrayBuffer: '{"a":{}}',
      deepNestedArr: '[[{"a":"b"},3],4]',
      deepNestedObj: '{"a":{"b":[1,2]}}',
    };
    validate = false;
    for (const id of Object.keys(itemsUnsupported)) {
      startCellEdit(id, "value", validate);
      checkCellUneditable(id, "value");
      checkCell(id, "value", expectedValStrings[id]);
    }

    info("Shut down the test");
    await toolbox.destroy();
    await extension.unload();
    await target.destroy();
  }
);
