/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const PAGE =
  "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

add_task(async function visible_false() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  async function background() {
    browser.menus.onShown.addListener(info => {
      browser.test.assertEq(
        "[]",
        JSON.stringify(info.menuIds),
        "Expected no menu items"
      );
      browser.test.sendMessage("done");
    });
    browser.menus.create({
      id: "create-visible-false",
      title: "invisible menu item",
      visible: false,
    });
    browser.menus.create({
      id: "update-without-params",
      title: "invisible menu item",
      visible: false,
    });
    await browser.menus.update("update-without-params", {});
    browser.menus.create({
      id: "update-visible-to-false",
      title: "initially visible menu item",
    });
    await browser.menus.update("update-visible-to-false", { visible: false });
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    background,
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  await openContextMenu();
  await extension.awaitMessage("done");
  await closeContextMenu();

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});

add_task(async function visible_true() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  async function background() {
    browser.menus.onShown.addListener(info => {
      browser.test.assertEq(
        `["update-to-true"]`,
        JSON.stringify(info.menuIds),
        "Expected no menu items"
      );
      browser.test.sendMessage("done");
    });
    browser.menus.create({
      id: "update-to-true",
      title: "invisible menu item",
      visible: false,
    });
    await browser.menus.update("update-to-true", { visible: true });
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    background,
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  await openContextMenu();
  await extension.awaitMessage("done");
  await closeContextMenu();

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
