/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function getVisibleChildrenIds(menuElem) {
  return Array.from(menuElem.children).filter(elem => !elem.hidden).map(elem => elem.id || elem.tagName);
}

function checkIsDefaultMenuItemVisible(visibleMenuItemIds) {
  // In this whole test file, we open a menu on a link. Assume that all
  // default menu items are shown if one link-specific menu item is shown.
  ok(visibleMenuItemIds.includes("context-openlink"),
     `The default 'Open Link in New Tab' menu item should be in ${visibleMenuItemIds}.`);
}

// Tests the following:
// - Calling overrideContext({}) during oncontextmenu forces the menu to only
//   show an extension's own items.
// - These menu items all appear in the root menu.
// - The usual extension filtering behavior (e.g. documentUrlPatterns and
//   targetUrlPatterns) is still applied; some menu items are therefore hidden.
// - Calling overrideContext({showDefaults:true}) causes the default menu items
//   to be shown, but only after the extension's.
// - overrideContext expires after the menu is opened once.
// - overrideContext can be called from shadow DOM.
add_task(async function overrideContext_in_extension_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.allow_eval_with_system_principal", true]]});

  function extensionTabScript() {
    document.addEventListener("contextmenu", () => {
      browser.menus.overrideContext({});
      browser.test.sendMessage("oncontextmenu_in_dom_part_1");
    }, {once: true});

    let shadowRoot = document.getElementById("shadowHost").attachShadow({mode: "open"});
    shadowRoot.innerHTML = `<a href="http://example.com/">Link</a>`;
    shadowRoot.firstChild.addEventListener("contextmenu", () => {
      browser.menus.overrideContext({});
      browser.test.sendMessage("oncontextmenu_in_shadow_dom");
    }, {once: true});

    browser.menus.create({
      id: "tab_1",
      title: "tab_1",
      documentUrlPatterns: [document.URL],
      onclick() {
        document.addEventListener("contextmenu", () => {
          // Verifies that last call takes precedence.
          browser.menus.overrideContext({showDefaults: false});
          browser.menus.overrideContext({showDefaults: true});
          browser.test.sendMessage("oncontextmenu_in_dom_part_2");
        }, {once: true});
        browser.test.sendMessage("onClicked_tab_1");
      },
    });
    browser.menus.create({
      id: "tab_2",
      title: "tab_2",
      onclick() {
        browser.test.sendMessage("onClicked_tab_2");
      },
    }, () => {
      browser.test.sendMessage("menu-registered");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus", "menus.overrideContext"],
    },
    files: {
      "tab.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <a href="http://example.com/">Link</a>
        <div id="shadowHost"></div>
        <script src="tab.js"></script>
      `,
      "tab.js": extensionTabScript,
    },
    background() {
      // Expected to match and thus be visible.
      browser.menus.create({id: "bg_1", title: "bg_1"});
      browser.menus.create({id: "bg_2", title: "bg_2", targetUrlPatterns: ["*://example.com/*"]});

      // Expected to not match and be hidden.
      browser.menus.create({id: "bg_3", title: "bg_3", targetUrlPatterns: ["*://nomatch/*"]});
      browser.menus.create({id: "bg_4", title: "bg_4", documentUrlPatterns: [document.URL]});

      browser.menus.onShown.addListener(info => {
        browser.test.assertEq("tab", info.viewType, "Expected viewType");
        browser.test.assertEq("bg_1,bg_2,tab_1,tab_2", info.menuIds.join(","), "Expected menu items.");
        browser.test.assertEq("all,link", info.contexts.sort().join(","), "Expected menu contexts");
        browser.test.sendMessage("onShown");
      });

      browser.tabs.create({url: "tab.html"});
    },
  });

  let otherExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    background() {
      browser.menus.create({id: "other_extension_item", title: "other_extension_item"}, () => {
        browser.test.sendMessage("other_extension_item_created");
      });
    },
  });
  await otherExtension.startup();
  await otherExtension.awaitMessage("other_extension_item_created");

  let extensionTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
  await extension.startup();
  // Must wait for the tab to have loaded completely before calling openContextMenu.
  await extensionTabPromise;
  await extension.awaitMessage("menu-registered");

  const EXPECTED_EXTENSION_MENU_IDS = [
    `${makeWidgetId(extension.id)}-menuitem-_bg_1`,
    `${makeWidgetId(extension.id)}-menuitem-_bg_2`,
    `${makeWidgetId(extension.id)}-menuitem-_tab_1`,
    `${makeWidgetId(extension.id)}-menuitem-_tab_2`,
  ];
  const OTHER_EXTENSION_MENU_ID =
    `${makeWidgetId(otherExtension.id)}-menuitem-_other_extension_item`;

  {
    // Tests overrideContext({})
    info("Expecting the menu to be replaced by overrideContext.");
    let menu = await openContextMenu("a");
    await extension.awaitMessage("oncontextmenu_in_dom_part_1");
    await extension.awaitMessage("onShown");

    Assert.deepEqual(
      getVisibleChildrenIds(menu),
      EXPECTED_EXTENSION_MENU_IDS,
      "Expected only extension menu items");

    let menuItems = menu.getElementsByAttribute("label", "tab_1");
    await closeExtensionContextMenu(menuItems[0]);
    await extension.awaitMessage("onClicked_tab_1");
  }

  {
    // Tests overrideContext({showDefaults:true}))
    info("Expecting the menu to be replaced by overrideContext, including default menu items.");
    let menu = await openContextMenu("a");
    await extension.awaitMessage("oncontextmenu_in_dom_part_2");
    await extension.awaitMessage("onShown");

    let visibleMenuItemIds = getVisibleChildrenIds(menu);
    Assert.deepEqual(
      visibleMenuItemIds.slice(0, EXPECTED_EXTENSION_MENU_IDS.length),
      EXPECTED_EXTENSION_MENU_IDS,
      "Expected extension menu items at the start.");

    checkIsDefaultMenuItemVisible(visibleMenuItemIds);

    is(visibleMenuItemIds[visibleMenuItemIds.length - 1], OTHER_EXTENSION_MENU_ID,
       "Other extension menu item should be at the end.");

    let menuItems = menu.getElementsByAttribute("label", "tab_2");
    await closeExtensionContextMenu(menuItems[0]);
    await extension.awaitMessage("onClicked_tab_2");
  }

  {
    // Tests that previous overrideContext call has been forgotten,
    // so the default behavior should occur (=move items into submenu).
    info("Expecting the default menu to be used when overrideContext is not called.");
    let menu = await openContextMenu("a");
    await extension.awaitMessage("onShown");

    checkIsDefaultMenuItemVisible(getVisibleChildrenIds(menu));

    let menuItems = menu.getElementsByAttribute("ext-type", "top-level-menu");
    is(menuItems.length, 1, "Expected top-level menu element for extension.");
    let topLevelExtensionMenuItem = menuItems[0];
    is(topLevelExtensionMenuItem.nextSibling, null, "Extension menu should be the last element.");

    const submenu = await openSubmenu(topLevelExtensionMenuItem);
    is(submenu, topLevelExtensionMenuItem.menupopup, "Correct submenu opened");

    Assert.deepEqual(
      getVisibleChildrenIds(submenu),
      EXPECTED_EXTENSION_MENU_IDS,
      "Extension menu items should be in the submenu by default.");

    await closeContextMenu();
  }

  {
    // Tests that overrideContext({}) can be used from a listener inside shadow DOM.
    let menu = await openContextMenu(() => content.document.getElementById("shadowHost").shadowRoot.firstChild);
    await extension.awaitMessage("oncontextmenu_in_shadow_dom");
    await extension.awaitMessage("onShown");

    Assert.deepEqual(
      getVisibleChildrenIds(menu),
      EXPECTED_EXTENSION_MENU_IDS,
      "Expected only extension menu items after overrideContext({}) in shadow DOM");

    await closeContextMenu();
  }

  // Unloading the extension will automatically close the extension's tab.html
  await extension.unload();
  await otherExtension.unload();
});

// Tests some edge cases:
// - overrideContext() is called without any menu registrations,
//   followed by a menu registration + menus.refresh..
// - overrideContext() is called and event.preventDefault() is also
//   called to stop the menu from appearing.
// - Open menu again and verify that the default menu behavior occurs.
add_task(async function overrideContext_sidebar_edge_cases() {
  function sidebarJs() {
    const TIME_BEFORE_MENU_SHOWN = Date.now();
    let count = 0;
    // eslint-disable-next-line mozilla/balanced-listeners
    document.addEventListener("contextmenu", event => {
      ++count;
      if (count === 1) {
        browser.menus.overrideContext({});
      } else if (count === 2) {
        browser.menus.overrideContext({});
        event.preventDefault(); // Prevent menu from being shown.

        // We are not expecting a menu. Wait for the time it took to show and
        // hide the previous menu, to check that no new menu appears.
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        setTimeout(() => {
          browser.test.sendMessage("stop_waiting_for_menu_shown", "timer_reached");
        }, Date.now() - TIME_BEFORE_MENU_SHOWN);
      } else if (count === 3) {
        // The overrideContext from the previous call should be forgotten.
        // Use the default behavior, i.e. show the default menu.
      } else {
        browser.test.fail(`Unexpected menu count: ${count}`);
      }

      browser.test.sendMessage("oncontextmenu_in_dom");
    });

    browser.menus.onShown.addListener(info => {
      browser.test.assertEq("sidebar", info.viewType, "Expected viewType");
      if (count === 1) {
        browser.test.assertEq("", info.menuIds.join(","), "Expected no items");
        browser.menus.create({id: "some_item", title: "some_item"}, () => {
          browser.test.sendMessage("onShown_1_and_menu_item_created");
        });
      } else if (count === 2) {
        browser.test.fail("onShown should not have fired when the menu is not shown.");
      } else if (count === 3) {
        browser.test.assertEq("some_item", info.menuIds.join(","), "Expected menu item");
        browser.test.sendMessage("onShown_3");
      } else {
        browser.test.fail(`Unexpected onShown at count: ${count}`);
      }
    });

    browser.test.onMessage.addListener(async msg => {
      browser.test.assertEq("refresh_menus", msg, "Expected message");
      browser.test.assertEq(1, count, "Expected at first menu test");
      await browser.menus.refresh();
      browser.test.sendMessage("menus_refreshed");
    });

    browser.menus.onHidden.addListener(() => {
      browser.test.sendMessage("onHidden", count);
    });

    browser.test.sendMessage("sidebar_ready");
  }
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary", // To automatically show sidebar on load.
    manifest: {
      permissions: ["menus", "menus.overrideContext"],
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    files: {
      "sidebar.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <a href="http://example.com/">Link</a>
        <script src="sidebar.js"></script>
      `,
      "sidebar.js": sidebarJs,
    },
    background() {
      browser.test.assertThrows(
        () => { browser.menus.overrideContext({someInvalidParameter: true}); },
        /Unexpected property "someInvalidParameter"/,
        "overrideContext should be available and the parameters be validated.");
      browser.test.sendMessage("bg_test_done");
    },
  });

  await extension.startup();
  await extension.awaitMessage("bg_test_done");
  await extension.awaitMessage("sidebar_ready");

  const EXPECTED_EXTENSION_MENU_ID =
    `${makeWidgetId(extension.id)}-menuitem-_some_item`;

  {
    // Checks that a menu can initially be empty and be updated.
    info("Expecting menu without items to appear and be updated after menus.refresh()");
    let menu = await openContextMenuInSidebar("a");
    await extension.awaitMessage("oncontextmenu_in_dom");
    await extension.awaitMessage("onShown_1_and_menu_item_created");
    Assert.deepEqual(getVisibleChildrenIds(menu), [], "Expected no items, initially");
    extension.sendMessage("refresh_menus");
    await extension.awaitMessage("menus_refreshed");
    Assert.deepEqual(getVisibleChildrenIds(menu), [EXPECTED_EXTENSION_MENU_ID], "Expected updated menu");
    await closeContextMenu(menu);
    is(await extension.awaitMessage("onHidden"), 1, "Menu hidden");
  }

  {
    // Trigger a context menu. The page has prevented the menu from being
    // shown, so the promise should not resolve.
    info("Expecting menu to not appear because of event.preventDefault()");
    let popupShowingPromise = openContextMenuInSidebar("a");
    await extension.awaitMessage("oncontextmenu_in_dom");
    is(await Promise.race([
      extension.awaitMessage("stop_waiting_for_menu_shown"),
      popupShowingPromise.then(() => "popup_shown"),
    ]), "timer_reached", "The menu should not be shown.");
  }

  {
    info("Expecting default menu to be shown when the menu is reopened after event.preventDefault()");
    let menu = await openContextMenuInSidebar("a");
    await extension.awaitMessage("oncontextmenu_in_dom");
    await extension.awaitMessage("onShown_3");
    let visibleMenuItemIds = getVisibleChildrenIds(menu);
    checkIsDefaultMenuItemVisible(visibleMenuItemIds);
    ok(visibleMenuItemIds.includes(EXPECTED_EXTENSION_MENU_ID), "Expected extension menu item");
    await closeContextMenu(menu);
    is(await extension.awaitMessage("onHidden"), 3, "Menu hidden");
  }

  await extension.unload();
});
