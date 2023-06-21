"use strict";

add_task(async function test_autoDiscardable() {
  let files = {
    "schema.json": JSON.stringify([
      {
        namespace: "experiments",
        functions: [
          {
            name: "unload",
            type: "function",
            async: true,
            description:
              "Unload the least recently used tab using Firefox's built-in tab unloader mechanism",
            parameters: [],
          },
        ],
      },
    ]),
    "parent.js": () => {
      const { TabUnloader } = ChromeUtils.importESModule(
        "resource:///modules/TabUnloader.sys.mjs"
      );
      const { ExtensionError } = ExtensionUtils;
      this.experiments = class extends ExtensionAPI {
        getAPI(context) {
          return {
            experiments: {
              async unload() {
                try {
                  await TabUnloader.unloadLeastRecentlyUsedTab(null);
                } catch (error) {
                  // We need to do this, otherwise failures won't bubble up to the test properly.
                  throw ExtensionError(error);
                }
              },
            },
          };
        }
      };
    },
  };

  async function background() {
    let firstTab = await browser.tabs.create({
      active: false,
      url: "https://example.org/",
    });

    // Make sure setting and getting works properly
    browser.test.assertTrue(
      firstTab.autoDiscardable,
      "autoDiscardable should always be true by default"
    );
    let result = await browser.tabs.update(firstTab.id, {
      autoDiscardable: false,
    });
    browser.test.assertFalse(
      result.autoDiscardable,
      "autoDiscardable should be false after setting it as such"
    );
    result = await browser.tabs.update(firstTab.id, {
      autoDiscardable: true,
    });
    browser.test.assertTrue(
      result.autoDiscardable,
      "autoDiscardable should be true after setting it as such"
    );
    result = await browser.tabs.update(firstTab.id, {
      autoDiscardable: false,
    });
    browser.test.assertFalse(
      result.autoDiscardable,
      "autoDiscardable should be false after setting it as such"
    );

    // Make sure the tab can't be unloaded when autoDiscardable is false
    await browser.experiments.unload();
    result = await browser.tabs.get(firstTab.id);
    browser.test.assertFalse(
      result.discarded,
      "Tab should not unload when autoDiscardable is false"
    );

    // Make sure the tab CAN be unloaded when autoDiscardable is true
    await browser.tabs.update(firstTab.id, {
      autoDiscardable: true,
    });
    await browser.experiments.unload();
    result = await browser.tabs.get(firstTab.id);
    browser.test.assertTrue(
      result.discarded,
      "Tab should unload when autoDiscardable is true"
    );

    // Make sure filtering for discardable tabs works properly
    result = await browser.tabs.query({ autoDiscardable: true });
    browser.test.assertEq(
      2,
      result.length,
      "tabs.query should return 2 when autoDiscardable is true "
    );
    await browser.tabs.update(firstTab.id, {
      autoDiscardable: false,
    });
    result = await browser.tabs.query({ autoDiscardable: true });
    browser.test.assertEq(
      1,
      result.length,
      "tabs.query should return 1 when autoDiscardable is false"
    );

    let onUpdatedPromise = {};
    onUpdatedPromise.promise = new Promise(
      resolve => (onUpdatedPromise.resolve = resolve)
    );

    // Make sure onUpdated works
    async function testOnUpdatedEvent(autoDiscardable) {
      browser.test.log(`Testing autoDiscardable = ${autoDiscardable}`);
      let onUpdated;
      let promise = new Promise(resolve => {
        onUpdated = (tabId, changeInfo, tabInfo) => {
          browser.test.assertEq(
            firstTab.id,
            tabId,
            "The updated tab's ID should match the correct tab"
          );
          browser.test.assertDeepEq(
            { autoDiscardable },
            changeInfo,
            "The updated tab's changeInfo should be correct"
          );
          browser.test.assertEq(
            tabInfo.autoDiscardable,
            autoDiscardable,
            "The updated tab's tabInfo should be correct"
          );
          resolve();
        };
      });
      browser.tabs.onUpdated.addListener(onUpdated, {
        properties: ["autoDiscardable"],
      });
      await browser.tabs.update(firstTab.id, { autoDiscardable });
      await promise;
      browser.tabs.onUpdated.removeListener(onUpdated);
    }

    await testOnUpdatedEvent(true);
    await testOnUpdatedEvent(false);

    await browser.tabs.remove(firstTab.id); // Cleanup
    browser.test.notifyPass("autoDiscardable");
  }
  let extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      permissions: ["tabs"],
      experiment_apis: {
        experiments: {
          schema: "schema.json",
          parent: {
            scopes: ["addon_parent"],
            script: "parent.js",
            paths: [["experiments"]],
          },
        },
      },
    },
    background,
    files,
  });
  await extension.startup();
  await extension.awaitFinish("autoDiscardable");
  await extension.unload();
});
