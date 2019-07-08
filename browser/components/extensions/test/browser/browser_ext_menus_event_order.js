/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const PAGE =
  "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";
add_task(async function test_menus_click_event_sequence() {
  async function background() {
    let events = [];

    browser.menus.onShown.addListener(() => {
      events.push("onShown");
    });
    browser.menus.onHidden.addListener(() => {
      events.push("onHidden");
      browser.test.sendMessage("event_sequence", events);
      events.length = 0;
    });

    browser.menus.create({
      title: "item in page menu",
      contexts: ["page"],
      onclick() {
        events.push("onclick parameter of page menu item");
      },
    });
    browser.menus.create(
      {
        title: "item in tools menu",
        contexts: ["tools_menu"],
        onclick() {
          events.push("onclick parameter of tools_menu menu item");
        },
      },
      () => {
        // The menus creation requests are expected to be handled in-order.
        // So when the callback for the last menu creation request is called,
        // we can assume that all menus have been registered.
        browser.test.sendMessage("created menus");
      }
    );
  }

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["menus"],
    },
  });
  await extension.startup();
  info("Waiting for events and menu items to be registered");
  await extension.awaitMessage("created menus");

  async function verifyResults(menuType) {
    info("Getting menu event info...");
    let events = await extension.awaitMessage("event_sequence");
    Assert.deepEqual(
      events,
      ["onShown", `onclick parameter of ${menuType} menu item`, "onHidden"],
      "Expected order of menus events"
    );
  }

  {
    info("Opening and closing page menu");
    const menu = await openContextMenu("body");
    const menuitem = menu.querySelector("menuitem[label='item in page menu']");
    ok(menuitem, "Page menu item should exist");
    await closeExtensionContextMenu(menuitem);
    await verifyResults("page");
  }

  {
    info("Opening and closing tools menu");
    const menu = await openToolsMenu();
    let menuitem = menu.querySelector("menuitem[label='item in tools menu']");
    ok(menuitem, "Tools menu item should exist");
    await closeToolsMenu(menuitem);
    await verifyResults("tools_menu");
  }

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});
