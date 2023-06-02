"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionParent: "resource://gre/modules/ExtensionParent.sys.mjs",
  Management: "resource://gre/modules/Extension.sys.mjs",
});

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

Services.prefs.setBoolPref("extensions.eventPages.enabled", true);

function getExtension(id, background, useAddonManager) {
  return ExtensionTestUtils.loadExtension({
    useAddonManager,
    manifest: {
      browser_specific_settings: { gecko: { id } },
      permissions: ["menus"],
      background: { persistent: false },
    },
    background,
  });
}

async function expectCached(extension, expect) {
  let { StartupCache } = ExtensionParent;
  let cached = await StartupCache.menus.get(extension.id);
  let createProperties = Array.from(cached.values());
  equal(cached.size, expect.length, "menus saved in cache");
  // The menus startupCache is a map and the order is significant
  // for recreating menus on startup.  Ensure that they are in
  // the expected order.  We only verify specific keys here rather
  // than all menu properties.
  for (let i in createProperties) {
    Assert.deepEqual(
      createProperties[i],
      expect[i],
      "expected cached properties exist"
    );
  }
}

function promiseExtensionEvent(wrapper, event) {
  return new Promise(resolve => {
    wrapper.extension.once(event, (kind, data) => {
      resolve(data);
    });
  });
}

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_menu_onInstalled() {
  async function background() {
    browser.runtime.onInstalled.addListener(async () => {
      const parentId = browser.menus.create({
        contexts: ["all"],
        title: "parent",
        id: "test-parent",
      });
      browser.menus.create({
        parentId,
        title: "click A",
        id: "test-click-a",
      });
      browser.menus.create(
        {
          parentId,
          title: "click B",
          id: "test-click-b",
        },
        () => {
          browser.test.sendMessage("onInstalled");
        }
      );
    });
    browser.menus.create(
      {
        contexts: ["tab"],
        title: "top-level",
        id: "test-top-level",
      },
      () => {
        browser.test.sendMessage("create", browser.runtime.lastError?.message);
      }
    );

    browser.test.onMessage.addListener(async msg => {
      browser.test.log(`onMessage ${msg}`);
      if (msg == "updatemenu") {
        await browser.menus.update("test-click-a", { title: "click updated" });
      } else if (msg == "removemenu") {
        await browser.menus.remove("test-click-b");
      } else if (msg == "removeall") {
        await browser.menus.removeAll();
      }
      browser.test.sendMessage("updated");
    });
  }

  const extension = getExtension(
    "test-persist@mochitest",
    background,
    "permanent"
  );

  await extension.startup();
  let lastError = await extension.awaitMessage("create");
  Assert.equal(lastError, undefined, "no error creating menu");
  await extension.awaitMessage("onInstalled");
  await extension.terminateBackground();

  await expectCached(extension, [
    {
      contexts: ["tab"],
      id: "test-top-level",
      title: "top-level",
    },
    { contexts: ["all"], id: "test-parent", title: "parent" },
    {
      id: "test-click-a",
      parentId: "test-parent",
      title: "click A",
    },
    {
      id: "test-click-b",
      parentId: "test-parent",
      title: "click B",
    },
  ]);

  await extension.wakeupBackground();
  lastError = await extension.awaitMessage("create");
  Assert.equal(
    lastError,
    "The menu id test-top-level already exists in menus.create.",
    "correct error creating menu"
  );

  await AddonTestUtils.promiseRestartManager();
  await extension.awaitStartup();

  // verify the startupcache
  await expectCached(extension, [
    {
      contexts: ["tab"],
      id: "test-top-level",
      title: "top-level",
    },
    { contexts: ["all"], id: "test-parent", title: "parent" },
    {
      id: "test-click-a",
      parentId: "test-parent",
      title: "click A",
    },
    {
      id: "test-click-b",
      parentId: "test-parent",
      title: "click B",
    },
  ]);

  equal(
    extension.extension.backgroundState,
    "stopped",
    "background is not running"
  );
  await extension.wakeupBackground();
  lastError = await extension.awaitMessage("create");
  Assert.equal(
    lastError,
    "The menu id test-top-level already exists in menus.create.",
    "correct error creating menu"
  );

  extension.sendMessage("updatemenu");
  await extension.awaitMessage("updated");
  await extension.terminateBackground();

  // Title change is cached
  await expectCached(extension, [
    {
      contexts: ["tab"],
      id: "test-top-level",
      title: "top-level",
    },
    { contexts: ["all"], id: "test-parent", title: "parent" },
    {
      id: "test-click-a",
      parentId: "test-parent",
      title: "click updated",
    },
    {
      id: "test-click-b",
      parentId: "test-parent",
      title: "click B",
    },
  ]);

  await extension.wakeupBackground();
  lastError = await extension.awaitMessage("create");
  Assert.equal(
    lastError,
    "The menu id test-top-level already exists in menus.create.",
    "correct error creating menu"
  );

  extension.sendMessage("removemenu");
  await extension.awaitMessage("updated");
  await extension.terminateBackground();

  // menu removed
  await expectCached(extension, [
    {
      contexts: ["tab"],
      id: "test-top-level",
      title: "top-level",
    },
    { contexts: ["all"], id: "test-parent", title: "parent" },
    {
      id: "test-click-a",
      parentId: "test-parent",
      title: "click updated",
    },
  ]);

  await extension.wakeupBackground();
  lastError = await extension.awaitMessage("create");
  Assert.equal(
    lastError,
    "The menu id test-top-level already exists in menus.create.",
    "correct error creating menu"
  );

  extension.sendMessage("removeall");
  await extension.awaitMessage("updated");
  await extension.terminateBackground();

  // menus removed
  await expectCached(extension, []);

  await extension.unload();
});

add_task(async function test_menu_nested() {
  async function background() {
    browser.test.onMessage.addListener(async (action, properties) => {
      browser.test.log(`onMessage ${action}`);
      switch (action) {
        case "create":
          await new Promise(resolve => {
            browser.menus.create(properties, resolve);
          });
          break;
        case "update":
          {
            let { id, ...update } = properties;
            await browser.menus.update(id, update);
          }
          break;
        case "remove":
          {
            let { id } = properties;
            await browser.menus.remove(id);
          }
          break;
        case "removeAll":
          await browser.menus.removeAll();
          break;
      }
      browser.test.sendMessage("updated");
    });
  }

  const extension = getExtension(
    "test-nesting@mochitest",
    background,
    "permanent"
  );
  await extension.startup();

  extension.sendMessage("create", {
    id: "first",
    contexts: ["all"],
    title: "first",
  });
  await extension.awaitMessage("updated");
  await expectCached(extension, [
    { contexts: ["all"], id: "first", title: "first" },
  ]);

  extension.sendMessage("create", {
    id: "second",
    contexts: ["all"],
    title: "second",
  });
  await extension.awaitMessage("updated");
  await expectCached(extension, [
    { contexts: ["all"], id: "first", title: "first" },
    { contexts: ["all"], id: "second", title: "second" },
  ]);

  extension.sendMessage("create", {
    id: "third",
    contexts: ["all"],
    title: "third",
    parentId: "first",
  });
  await extension.awaitMessage("updated");
  await expectCached(extension, [
    { contexts: ["all"], id: "first", title: "first" },
    { contexts: ["all"], id: "second", title: "second" },
    {
      contexts: ["all"],
      id: "third",
      parentId: "first",
      title: "third",
    },
  ]);

  extension.sendMessage("create", {
    id: "fourth",
    contexts: ["all"],
    title: "fourth",
  });
  await extension.awaitMessage("updated");
  await expectCached(extension, [
    { contexts: ["all"], id: "first", title: "first" },
    { contexts: ["all"], id: "second", title: "second" },
    {
      contexts: ["all"],
      id: "third",
      parentId: "first",
      title: "third",
    },
    { contexts: ["all"], id: "fourth", title: "fourth" },
  ]);

  extension.sendMessage("update", {
    id: "first",
    parentId: "second",
  });
  await extension.awaitMessage("updated");
  await expectCached(extension, [
    { contexts: ["all"], id: "second", title: "second" },
    { contexts: ["all"], id: "fourth", title: "fourth" },
    {
      contexts: ["all"],
      id: "first",
      title: "first",
      parentId: "second",
    },
    {
      contexts: ["all"],
      id: "third",
      parentId: "first",
      title: "third",
    },
  ]);

  await AddonTestUtils.promiseShutdownManager();
  // We need to attach an event listener before the
  // startup event is emitted.  Fortunately, we
  // emit via Management before emitting on extension.
  let promiseMenus;
  Management.once("startup", (kind, ext) => {
    info(`management ${kind} ${ext.id}`);
    promiseMenus = promiseExtensionEvent(
      { extension: ext },
      "webext-menus-created"
    );
  });
  await AddonTestUtils.promiseStartupManager();
  await extension.awaitStartup();
  await extension.wakeupBackground();

  await expectCached(extension, [
    { contexts: ["all"], id: "second", title: "second" },
    { contexts: ["all"], id: "fourth", title: "fourth" },
    {
      contexts: ["all"],
      id: "first",
      title: "first",
      parentId: "second",
    },
    {
      contexts: ["all"],
      id: "third",
      parentId: "first",
      title: "third",
    },
  ]);
  // validate nesting
  let menus = await promiseMenus;
  equal(menus.get("first").parentId, "second", "menuitem parent is correct");
  equal(
    menus.get("second").children.length,
    1,
    "menuitem parent has correct number of children"
  );
  equal(
    menus.get("second").root.children.length,
    2, // second and forth
    "menuitem root has correct number of children"
  );

  extension.sendMessage("remove", {
    id: "second",
  });
  await extension.awaitMessage("updated");
  await expectCached(extension, [
    { contexts: ["all"], id: "fourth", title: "fourth" },
  ]);

  extension.sendMessage("removeAll");
  await extension.awaitMessage("updated");
  await expectCached(extension, []);

  await extension.unload();
});
