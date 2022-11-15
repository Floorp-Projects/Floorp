/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function getTargetElement_in_extension_tab() {
  async function background() {
    browser.menus.onShown.addListener(info => {
      let elem = browser.menus.getTargetElement(info.targetElementId);
      browser.test.assertEq(
        null,
        elem,
        "should not get element of tab content in background"
      );

      // By using getViews() here, we verify that the targetElementId can
      // synchronously be mapped to a valid element in a different tab
      // during the onShown event.
      let [tabGlobal] = browser.extension.getViews({ type: "tab" });
      elem = tabGlobal.browser.menus.getTargetElement(info.targetElementId);
      browser.test.assertEq(
        "BUTTON",
        elem.tagName,
        "should get element in tab content"
      );
      browser.test.sendMessage("elementChecked");
    });

    browser.tabs.create({ url: "tab.html" });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    files: {
      "tab.html": `<!DOCTYPE html><meta charset="utf-8"><button>Button in tab</button>`,
    },
    background,
  });

  let extensionTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    null,
    true
  );
  await extension.startup();
  // Must wait for the tab to have loaded completely before calling openContextMenu.
  await extensionTabPromise;
  await openContextMenu("button");
  await extension.awaitMessage("elementChecked");
  await closeContextMenu();

  // Unloading the extension will automatically close the extension's tab.html
  await extension.unload();
});

add_task(async function getTargetElement_in_extension_tab_on_click() {
  // Similar to getTargetElement_in_extension_tab, except we check whether
  // calling getTargetElement in onClicked results in the expected behavior.
  async function background() {
    browser.menus.onClicked.addListener(info => {
      let [tabGlobal] = browser.extension.getViews({ type: "tab" });
      let elem = tabGlobal.browser.menus.getTargetElement(info.targetElementId);
      browser.test.assertEq(
        "BUTTON",
        elem.tagName,
        "should get element in tab content on click"
      );
      browser.test.sendMessage("elementClicked");
    });

    browser.menus.create({ title: "click here" }, () => {
      browser.tabs.create({ url: "tab.html" });
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    files: {
      "tab.html": `<!DOCTYPE html><meta charset="utf-8"><button>Button in tab</button>`,
    },
    background,
  });

  let extensionTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    null,
    true
  );
  await extension.startup();
  await extensionTabPromise;
  let menu = await openContextMenu("button");
  let menuItem = menu.getElementsByAttribute("label", "click here")[0];
  await closeExtensionContextMenu(menuItem);
  await extension.awaitMessage("elementClicked");

  await extension.unload();
});

add_task(async function getTargetElement_in_browserAction_popup() {
  async function background() {
    browser.menus.onShown.addListener(info => {
      let elem = browser.menus.getTargetElement(info.targetElementId);
      browser.test.assertEq(
        null,
        elem,
        "should not get element of popup content in background"
      );

      let [popupGlobal] = browser.extension.getViews({ type: "popup" });
      elem = popupGlobal.browser.menus.getTargetElement(info.targetElementId);
      browser.test.assertEq(
        "BUTTON",
        elem.tagName,
        "should get element in popup content"
      );
      browser.test.sendMessage("popupChecked");
    });

    // Ensure that onShown is registered (workaround for bug 1300234):
    await browser.menus.removeAll();
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
      },
    },
    files: {
      "popup.html": `<!DOCTYPE html><meta charset="utf-8"><button>Button in popup</button>`,
    },
    background,
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  await clickBrowserAction(extension);
  await openContextMenuInPopup(extension, "button");
  await extension.awaitMessage("popupChecked");
  await closeContextMenu();
  await closeBrowserAction(extension);

  await extension.unload();
});

add_task(async function getTargetElement_in_sidebar_panel() {
  async function sidebarJs() {
    browser.menus.onShown.addListener(info => {
      let expected = document.querySelector("button");
      let elem = browser.menus.getTargetElement(info.targetElementId);
      browser.test.assertEq(
        expected,
        elem,
        "should get element in sidebar content"
      );
      browser.test.sendMessage("done");
    });

    // Ensure that onShown is registered (workaround for bug 1300234):
    await browser.menus.removeAll();
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary", // To automatically show sidebar on load.
    manifest: {
      permissions: ["menus"],
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    files: {
      "sidebar.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <button>Button in sidebar</button>
        <script src="sidebar.js"></script>
      `,
      "sidebar.js": sidebarJs,
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  let sidebarMenu = await openContextMenuInSidebar("button");
  await extension.awaitMessage("done");
  await closeContextMenu(sidebarMenu);

  await extension.unload();
});
