/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const PAGE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

// Registers a context menu using menus.create(menuCreateParams) and checks
// whether the menus.onShown and menus.onHidden events are fired as expected.
// doOpenMenu must open the menu and its returned promise must resolve after the
// menu is shown. Similarly, doCloseMenu must hide the menu.
async function testShowHideEvent({menuCreateParams, doOpenMenu, doCloseMenu,
                                  expectedShownEvent}) {
  async function background() {
    function awaitMessage(expectedId) {
      return new Promise(resolve => {
        browser.test.log(`Waiting for message: ${expectedId}`);
        browser.test.onMessage.addListener(function listener(id, msg) {
          browser.test.assertEq(expectedId, id, "Expected message");
          browser.test.onMessage.removeListener(listener);
          resolve(msg);
        });
      });
    }

    let menuCreateParams = await awaitMessage("create-params");

    let shownEvents = [];
    let hiddenEvents = [];

    browser.menus.onShown.addListener(event => shownEvents.push(event));
    browser.menus.onHidden.addListener(event => hiddenEvents.push(event));

    const [tab] = await browser.tabs.query({active: true});
    await browser.pageAction.show(tab.id);

    let menuId;
    await new Promise(resolve => {
      menuId = browser.menus.create(menuCreateParams, resolve);
    });
    browser.test.assertEq(0, shownEvents.length, "no onShown before menu");
    browser.test.assertEq(0, hiddenEvents.length, "no onHidden before menu");
    browser.test.sendMessage("menu-registered", menuId);

    await awaitMessage("assert-menu-shown");
    browser.test.assertEq(1, shownEvents.length, "expected onShown");
    browser.test.assertEq(0, hiddenEvents.length, "no onHidden before closing");
    browser.test.sendMessage("onShown-event-data", shownEvents[0]);

    await awaitMessage("assert-menu-hidden");
    browser.test.assertEq(1, shownEvents.length, "expected no more onShown");
    browser.test.assertEq(1, hiddenEvents.length, "expected onHidden");
    browser.test.sendMessage("onHidden-event-data", hiddenEvents[0]);
  }

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      page_action: {},
      browser_action: {},
      permissions: ["menus"],
    },
  });
  await extension.startup();
  extension.sendMessage("create-params", menuCreateParams);
  let menuId = await extension.awaitMessage("menu-registered");

  await doOpenMenu(extension);
  extension.sendMessage("assert-menu-shown");
  let shownEvent = await extension.awaitMessage("onShown-event-data");

  // menuCreateParams.id is not set, therefore a numeric ID is generated.
  expectedShownEvent.menuIds = [menuId];
  Assert.deepEqual(shownEvent, expectedShownEvent, "expected onShown info");

  await doCloseMenu();
  extension.sendMessage("assert-menu-hidden");
  let hiddenEvent = await extension.awaitMessage("onHidden-event-data");
  is(hiddenEvent, undefined, "expected no event data for onHidden event");

  await extension.unload();
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function test_no_show_hide_without_menu_item() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let events = [];
      browser.menus.onShown.addListener(data => events.push(data));
      browser.menus.onHidden.addListener(() => events.push("onHidden"));
      browser.test.onMessage.addListener(() => {
        browser.test.assertEq("[]", JSON.stringify(events),
          "Should not have any events when the context menu does not match");
        browser.test.notifyPass("done listening to menu events");
      });

      browser.menus.create({
        title: "never shown",
        documentUrlPatterns: ["*://url-pattern-that-never-matches/*"],
        contexts: ["all"],
      });
    },
    manifest: {
      permissions: ["menus"],
    },
  });

  await extension.startup();

  // Run another context menu test where onShown/onHidden will fire.
  await testShowHideEvent({
    menuCreateParams: {
      title: "any menu item",
      contexts: ["all"],
    },
    expectedShownEvent: {
      contexts: ["page", "all"],
    },
    async doOpenMenu() {
      await openContextMenu("body");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });

  // Now the menu has been shown and hidden, and in another extension the
  // onShown/onHidden events have been dispatched.
  // If the the first extension has not received any events by now, we can be
  // confident that the onShown/onHidden events are not unexpectedly triggered.
  extension.sendMessage("check menu events");
  await extension.awaitFinish("done listening to menu events");
  await extension.unload();
});

add_task(async function test_show_hide_pageAction() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "pageAction item",
      contexts: ["page_action"],
    },
    expectedShownEvent: {
      contexts: ["page_action", "all"],
    },
    async doOpenMenu(extension) {
      await openActionContextMenu(extension, "page");
    },
    async doCloseMenu() {
      await closeActionContextMenu(null, "page");
    },
  });
});

add_task(async function test_show_hide_browserAction() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "browserAction item",
      contexts: ["browser_action"],
    },
    expectedShownEvent: {
      contexts: ["browser_action", "all"],
    },
    async doOpenMenu(extension) {
      await openActionContextMenu(extension, "browser");
    },
    async doCloseMenu() {
      await closeActionContextMenu();
    },
  });
});

add_task(async function test_show_hide_tab() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "tab menu item",
      contexts: ["tab"],
    },
    expectedShownEvent: {
      contexts: ["tab"],
    },
    async doOpenMenu() {
      await openTabContextMenu();
    },
    async doCloseMenu() {
      await closeTabContextMenu();
    },
  });
});

add_task(async function test_show_hide_tools_menu() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "menu item",
      contexts: ["tools_menu"],
    },
    expectedShownEvent: {
      contexts: ["tools_menu"],
    },
    async doOpenMenu() {
      await openToolsMenu();
    },
    async doCloseMenu() {
      await closeToolsMenu();
    },
  });
});

add_task(async function test_show_hide_page() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "page menu item",
      contexts: ["page"],
    },
    expectedShownEvent: {
      contexts: ["page", "all"],
    },
    async doOpenMenu() {
      await openContextMenu("body");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

add_task(async function test_show_hide_frame() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "subframe menu item",
      contexts: ["frame"],
    },
    expectedShownEvent: {
      contexts: ["frame", "all"],
    },
    async doOpenMenu() {
      await openContextMenuInFrame("frame");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

add_task(async function test_show_hide_password() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "password item",
      contexts: ["password"],
    },
    expectedShownEvent: {
      contexts: ["password", "all"],
    },
    async doOpenMenu() {
      await openContextMenu("#password");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

add_task(async function test_show_hide_image_link() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "image item",
      contexts: ["image"],
    },
    expectedShownEvent: {
      contexts: ["image", "link", "all"],
    },
    async doOpenMenu() {
      await openContextMenu("#img-wrapped-in-link");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

add_task(async function test_show_hide_editable_selection() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "editable item",
      contexts: ["editable"],
    },
    expectedShownEvent: {
      contexts: ["editable", "selection", "all"],
    },
    async doOpenMenu() {
      // Select lots of text in the test page before opening the menu.
      await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
        let node = content.document.getElementById("editabletext");
        node.select();
        node.focus();
      });

      await openContextMenu("#editabletext");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

// TODO(robwu): Add test coverage for contexts audio, video (bug 1398542).

