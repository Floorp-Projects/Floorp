"use strict";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.eventPages.enabled", true]],
  });
});

function getExtension(background, useAddonManager) {
  return ExtensionTestUtils.loadExtension({
    useAddonManager,
    manifest: {
      browser_action: {
        default_area: "navbar",
      },
      permissions: ["menus"],
      background: { persistent: false },
    },
    background,
  });
}

add_task(async function test_menu_create_id() {
  let waitForConsole = new Promise(resolve => {
    SimpleTest.waitForExplicitFinish();
    SimpleTest.monitorConsole(resolve, [
      // Callback exists, lastError is checked, so we should not see this logged.
      {
        message:
          /Unchecked lastError value: Error: menus.create requires an id for non-persistent background scripts./,
        forbid: true,
      },
    ]);
  });

  function background() {
    // Event pages require ID
    browser.menus.create(
      { contexts: ["browser_action"], title: "parent" },
      () => {
        browser.test.assertEq(
          "menus.create requires an id for non-persistent background scripts.",
          browser.runtime.lastError?.message,
          "lastError message for missing id"
        );
        browser.test.sendMessage("done");
      }
    );
  }
  const extension = getExtension(background);
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();

  SimpleTest.endMonitorConsole();
  await waitForConsole;
});

add_task(async function test_menu_onclick() {
  async function background() {
    const contexts = ["browser_action"];

    const parentId = browser.menus.create({
      contexts,
      title: "parent",
      id: "test-parent",
    });
    browser.menus.create({ parentId, title: "click A", id: "test-click" });

    browser.menus.onClicked.addListener((info, tab) => {
      browser.test.sendMessage("click", { info, tab });
    });
    browser.runtime.onSuspend.addListener(() => {
      browser.test.sendMessage("suspended-test_menu_onclick");
    });
  }

  const extension = getExtension(background);

  await extension.startup();
  await extension.terminateBackground(); // Simulated suspend on idle.
  await extension.awaitMessage("suspended-test_menu_onclick");

  // The background is now suspended, test that a menu click starts it.
  const kind = "browser";

  const menu = await openActionContextMenu(extension, kind);
  const [submenu] = menu.children;
  const popup = await openSubmenu(submenu);

  await closeActionContextMenu(popup.firstElementChild, kind);
  const clicked = await extension.awaitMessage("click");
  is(clicked.info.pageUrl, "about:blank", "Click info pageUrl is correct");
  Assert.greater(clicked.tab.id, -1, "Click event tab ID is correct");

  await extension.unload();
});

add_task(async function test_menu_onshown() {
  async function background() {
    const contexts = ["browser_action"];

    const parentId = browser.menus.create({
      contexts,
      title: "parent",
      id: "test-parent",
    });
    browser.menus.create({ parentId, title: "click A", id: "test-click" });

    browser.menus.onClicked.addListener((info, tab) => {
      browser.test.sendMessage("click", { info, tab });
    });
    browser.menus.onShown.addListener((info, tab) => {
      browser.test.sendMessage("shown", { info, tab });
    });
    browser.menus.onHidden.addListener((info, tab) => {
      browser.test.sendMessage("hidden", { info, tab });
    });
    browser.runtime.onSuspend.addListener(() => {
      browser.test.sendMessage("suspended-test_menu_onshown");
    });
  }

  const extension = getExtension(background);

  await extension.startup();
  await extension.terminateBackground(); // Simulated suspend on idle.
  await extension.awaitMessage("suspended-test_menu_onshown");

  // The background is now suspended, test that showing a menu starts it.
  const kind = "browser";

  const menu = await openActionContextMenu(extension, kind);
  const [submenu] = menu.children;
  const popup = await openSubmenu(submenu);
  await extension.awaitMessage("shown");

  await closeActionContextMenu(popup.firstElementChild, kind);
  await extension.awaitMessage("hidden");
  // The click still should work after the background was restarted.
  const clicked = await extension.awaitMessage("click");
  is(clicked.info.pageUrl, "about:blank", "Click info pageUrl is correct");
  Assert.greater(clicked.tab.id, -1, "Click event tab ID is correct");

  await extension.unload();
});

add_task(async function test_actions_context_menu() {
  function background() {
    browser.contextMenus.create({
      id: "my_browser_action",
      title: "open_browser_action",
      contexts: ["all"],
      command: "_execute_browser_action",
    });
    browser.contextMenus.onClicked.addListener(() => {
      browser.test.fail(`menu onClicked should not have been received`);
    });
    browser.test.sendMessage("ready");
  }

  function testScript() {
    window.onload = () => {
      browser.test.sendMessage("test-opened", true);
    };
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "contextMenus commands",
      permissions: ["contextMenus"],
      browser_action: {
        default_title: "Test BrowserAction",
        default_popup: "test.html",
        default_area: "navbar",
        browser_style: true,
      },
      background: { persistent: false },
    },
    background,
    files: {
      "test.html": `<!DOCTYPE html><meta charset="utf-8"><script src="test.js"></script>`,
      "test.js": testScript,
    },
  });

  async function testContext(id) {
    const menu = await openContextMenu();
    const items = menu.getElementsByAttribute("label", id);
    is(items.length, 1, `exactly one menu item found`);
    await closeExtensionContextMenu(items[0]);
    return extension.awaitMessage("test-opened");
  }

  await extension.startup();
  await extension.awaitMessage("ready");
  await extension.terminateBackground();

  // open a page so context menu works
  const PAGE =
    "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html?test=commands";
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  ok(
    await testContext("open_browser_action"),
    "_execute_browser_action worked"
  );

  await BrowserTestUtils.removeTab(tab);
  let ext = WebExtensionPolicy.getByID(extension.id).extension;
  is(ext.backgroundState, "stopped", "background is not running");

  await extension.unload();
});

add_task(async function test_menu_create_id_reuse() {
  let waitForConsole = new Promise(resolve => {
    SimpleTest.waitForExplicitFinish();
    SimpleTest.monitorConsole(resolve, [
      // Callback exists, lastError is checked, so we should not see this logged.
      {
        message:
          /Unchecked lastError value: Error: menus.create requires an id for non-persistent background scripts./,
        forbid: true,
      },
    ]);
  });

  function background() {
    browser.menus.create(
      {
        contexts: ["browser_action"],
        title: "click A",
        id: "test-click",
      },
      () => {
        browser.test.sendMessage("create", browser.runtime.lastError?.message);
      }
    );
    browser.test.onMessage.addListener(msg => {
      browser.test.assertEq("add-again", msg, "expected msg");
      browser.menus.create(
        {
          contexts: ["browser_action"],
          title: "click A",
          id: "test-click",
        },
        () => {
          browser.test.assertEq(
            "The menu id test-click already exists in menus.create.",
            browser.runtime.lastError?.message,
            "lastError message for missing id"
          );
          browser.test.sendMessage("done");
        }
      );
    });
  }
  const extension = getExtension(background, "temporary");
  await extension.startup();
  let lastError = await extension.awaitMessage("create");
  Assert.equal(lastError, undefined, "no error creating menu");
  extension.sendMessage("add-again");
  await extension.awaitMessage("done");
  await extension.terminateBackground();
  await extension.wakeupBackground();
  lastError = await extension.awaitMessage("create");
  Assert.equal(
    lastError,
    "The menu id test-click already exists in menus.create.",
    "lastError using duplicate ID"
  );
  await extension.unload();

  SimpleTest.endMonitorConsole();
  await waitForConsole;
});
