// /* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
// /* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const PAGE =
  "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

add_task(async function test_buttons() {
  const manifest = {
    permissions: ["menus"],
  };

  function background() {
    function onclick(info) {
      browser.test.sendMessage("click", info);
    }
    browser.menus.create({ title: "modify", onclick }, () => {
      browser.test.sendMessage("ready");
    });
  }

  const extension = ExtensionTestUtils.loadExtension({ manifest, background });
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  await extension.startup();
  await extension.awaitMessage("ready");

  for (let i of [0, 1, 2]) {
    const menu = await openContextMenu();
    const items = menu.getElementsByAttribute("label", "modify");
    await closeExtensionContextMenu(items[0], { button: i });
    const info = await extension.awaitMessage("click");
    is(info.button, i, `Button value should be ${i}`);
  }

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function test_submenu() {
  function background() {
    browser.menus.onClicked.addListener(info => {
      browser.test.assertEq("child", info.menuItemId, "expected menu item");
      browser.test.sendMessage("clicked_button", info.button);
    });
    browser.menus.create({
      id: "parent",
      title: "parent",
    });
    browser.menus.create(
      {
        id: "child",
        parentId: "parent",
        title: "child",
      },
      () => browser.test.sendMessage("ready")
    );
  }
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    background,
  });
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  await extension.startup();
  await extension.awaitMessage("ready");

  for (let button of [0, 1, 2]) {
    const menu = await openContextMenu();
    const parentItem = menu.getElementsByAttribute("label", "parent")[0];
    const submenu = await openSubmenu(parentItem);
    const childItem = submenu.firstElementChild;
    // This should not trigger a click event, thus we intentionally turn off
    // this a11y check as <menu> containers are not expected to be interactive.
    AccessibilityUtils.setEnv({
      mustHaveAccessibleRule: false,
    });
    await EventUtils.synthesizeMouseAtCenter(parentItem, { button });
    AccessibilityUtils.resetEnv();
    await closeExtensionContextMenu(childItem, { button });
    is(
      await extension.awaitMessage("clicked_button"),
      button,
      "Expected button"
    );
  }

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function test_disabled_item() {
  function background() {
    browser.menus.onHidden.addListener(() =>
      browser.test.sendMessage("onHidden")
    );
    browser.menus.create(
      {
        title: "disabled_item",
        enabled: false,
        onclick(info) {
          browser.test.fail(
            `Unexpected click on disabled_item, button=${info.button}`
          );
        },
      },
      () => browser.test.sendMessage("ready")
    );
  }
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    background,
  });
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  await extension.startup();
  await extension.awaitMessage("ready");

  for (let button of [0, 1, 2]) {
    const menu = await openContextMenu();
    const items = menu.getElementsByAttribute("label", "disabled_item");
    // We intentionally turn off this a11y check, because the following click
    // is targeting a disabled control to confirm the click event won't come through.
    // It is not meant to be interactive and is not expected to be accessible:
    AccessibilityUtils.setEnv({
      mustBeEnabled: false,
    });
    await EventUtils.synthesizeMouseAtCenter(items[0], { button });
    AccessibilityUtils.resetEnv();
    await closeContextMenu();
    await extension.awaitMessage("onHidden");
  }

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});
