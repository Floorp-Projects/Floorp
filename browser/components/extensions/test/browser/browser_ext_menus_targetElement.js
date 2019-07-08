/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const PAGE =
  "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

// Loads an extension that records menu visibility events in the current tab.
// The returned extension has two helper functions "openContextMenu" and
// "checkIsValid" that are used to verify the behavior of targetElementId.
async function loadExtensionAndTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  gBrowser.selectedTab = tab;

  function contentScript() {
    browser.test.onMessage.addListener(
      (msg, targetElementId, expectedSelector, description) => {
        browser.test.assertEq("checkIsValid", msg, "Expected message");

        let expected = expectedSelector
          ? document.querySelector(expectedSelector)
          : null;
        let elem = browser.menus.getTargetElement(targetElementId);
        browser.test.assertEq(expected, elem, description);
        browser.test.sendMessage("checkIsValidDone");
      }
    );
  }

  async function background() {
    browser.menus.onShown.addListener(async (info, tab) => {
      browser.test.sendMessage("onShownMenu", info.targetElementId);
    });
    await browser.tabs.executeScript({ file: "contentScript.js" });
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus", "http://mochi.test/*"],
    },
    background,
    files: {
      "contentScript.js": contentScript,
    },
  });

  extension.openAndCloseMenu = async selector => {
    await openContextMenu(selector);
    let targetElementId = await extension.awaitMessage("onShownMenu");
    await closeContextMenu();
    return targetElementId;
  };

  extension.checkIsValid = async (
    targetElementId,
    expectedSelector,
    description
  ) => {
    extension.sendMessage(
      "checkIsValid",
      targetElementId,
      expectedSelector,
      description
    );
    await extension.awaitMessage("checkIsValidDone");
  };

  await extension.startup();
  await extension.awaitMessage("ready");
  return { extension, tab };
}

// Tests that info.targetElementId is only available with the right permissions.
add_task(async function required_permission() {
  let { extension, tab } = await loadExtensionAndTab();

  // Load another extension to verify that the permission from the first
  // extension does not enable the "targetElementId" parameter.
  function background() {
    browser.contextMenus.onShown.addListener((info, tab) => {
      browser.test.assertEq(
        undefined,
        info.targetElementId,
        "targetElementId requires permission"
      );
      browser.test.sendMessage("onShown");
    });
    browser.contextMenus.onClicked.addListener(async info => {
      browser.test.assertEq(
        undefined,
        info.targetElementId,
        "targetElementId requires permission"
      );
      const code = `
        browser.test.assertEq(undefined, browser.menus, "menus API requires permission in content script");
        browser.test.assertEq(undefined, browser.contextMenus, "contextMenus API not available in content script.");
      `;
      await browser.tabs.executeScript({ code });
      browser.test.sendMessage("onClicked");
    });
    browser.contextMenus.create({ title: "menu for page" }, () => {
      browser.test.sendMessage("ready");
    });
  }
  let extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["contextMenus", "http://mochi.test/*"],
    },
    background,
  });
  await extension2.startup();
  await extension2.awaitMessage("ready");

  let menu = await openContextMenu();
  await extension.awaitMessage("onShownMenu");
  let menuItem = menu.getElementsByAttribute("label", "menu for page")[0];
  await closeExtensionContextMenu(menuItem);

  await extension2.awaitMessage("onShown");
  await extension2.awaitMessage("onClicked");
  await extension2.unload();

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

// Tests that the basic functionality works as expected.
add_task(async function getTargetElement_in_page() {
  let { extension, tab } = await loadExtensionAndTab();

  for (let selector of ["#img1", "#link1", "#password"]) {
    let targetElementId = await extension.openAndCloseMenu(selector);
    ok(
      Number.isInteger(targetElementId),
      `targetElementId (${targetElementId}) should be an integer for ${selector}`
    );

    await extension.checkIsValid(
      targetElementId,
      selector,
      `Expected target to match ${selector}`
    );
  }

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function getTargetElement_in_frame() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  gBrowser.selectedTab = tab;

  async function background() {
    let targetElementId;
    browser.menus.onShown.addListener(async (info, tab) => {
      browser.test.assertTrue(
        info.frameUrl.endsWith("context_frame.html"),
        `Expected frame ${info.frameUrl}`
      );
      targetElementId = info.targetElementId;
      let elem = browser.menus.getTargetElement(targetElementId);
      browser.test.assertEq(
        null,
        elem,
        "should not find page element in extension's background"
      );

      await browser.tabs.executeScript(tab.id, {
        code: `{
          let elem = browser.menus.getTargetElement(${targetElementId});
          browser.test.assertEq(null, elem, "should not find element from different frame");
        }`,
      });

      await browser.tabs.executeScript(tab.id, {
        frameId: info.frameId,
        code: `{
          let elem = browser.menus.getTargetElement(${targetElementId});
          browser.test.assertEq(document.body, elem, "should find the target element in the frame");
        }`,
      });
      browser.test.sendMessage("pageAndFrameChecked");
    });

    browser.menus.onClicked.addListener(info => {
      browser.test.assertEq(
        targetElementId,
        info.targetElementId,
        "targetElementId in onClicked must match onShown."
      );
      browser.test.sendMessage("onClickedChecked");
    });

    browser.menus.create(
      { title: "menu for frame", contexts: ["frame"] },
      () => {
        browser.test.sendMessage("ready");
      }
    );
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus", "http://mochi.test/*"],
    },
    background,
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  let menu = await openContextMenuInFrame("#frame");
  await extension.awaitMessage("pageAndFrameChecked");
  let menuItem = menu.getElementsByAttribute("label", "menu for frame")[0];
  await closeExtensionContextMenu(menuItem);
  await extension.awaitMessage("onClickedChecked");

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

// Test that getTargetElement does not return a detached element.
add_task(async function getTargetElement_after_removing_element() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  function background() {
    function contentScript(targetElementId) {
      let expectedElem = document.getElementById("edit-me");
      let { nextElementSibling } = expectedElem;

      let elem = browser.menus.getTargetElement(targetElementId);
      browser.test.assertEq(
        expectedElem,
        elem,
        "Expected target element before element removal"
      );

      expectedElem.remove();
      elem = browser.menus.getTargetElement(targetElementId);
      browser.test.assertEq(
        null,
        elem,
        "Expected no target element after element removal."
      );

      nextElementSibling.insertAdjacentElement("beforebegin", expectedElem);
      elem = browser.menus.getTargetElement(targetElementId);
      browser.test.assertEq(
        expectedElem,
        elem,
        "Expected target element after element restoration."
      );
    }
    browser.menus.onClicked.addListener(async (info, tab) => {
      const code = `(${contentScript})(${info.targetElementId})`;
      browser.test.log(code);
      await browser.tabs.executeScript(tab.id, { code });
      browser.test.sendMessage("checkedRemovedElement");
    });
    browser.menus.create({ title: "some menu item" }, () => {
      browser.test.sendMessage("ready");
    });
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus", "http://mochi.test/*"],
    },
    background,
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  let menu = await openContextMenu("#edit-me");
  let menuItem = menu.getElementsByAttribute("label", "some menu item")[0];
  await closeExtensionContextMenu(menuItem);
  await extension.awaitMessage("checkedRemovedElement");
  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

// Tests whether targetElementId expires after opening a new menu.
add_task(async function expireTargetElement() {
  let { extension, tab } = await loadExtensionAndTab();

  // Open the menu once to get the first element ID.
  let targetElementId = await extension.openAndCloseMenu("#longtext");

  // Open another menu. The previous ID should expire.
  await extension.openAndCloseMenu("#longtext");
  await extension.checkIsValid(
    targetElementId,
    null,
    `Expected initial target ID to expire after opening another menu`
  );

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

// Tests whether targetElementId of different tabs are independent.
add_task(async function independentMenusInDifferentTabs() {
  let { extension, tab } = await loadExtensionAndTab();

  let targetElementId = await extension.openAndCloseMenu("#longtext");

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE + "?");
  gBrowser.selectedTab = tab2;

  let targetElementId2 = await extension.openAndCloseMenu("#editabletext");

  await extension.checkIsValid(
    targetElementId2,
    null,
    "targetElementId from different tab should not resolve."
  );
  await extension.checkIsValid(
    targetElementId,
    "#longtext",
    "Expected getTargetElement to work after closing a menu in another tab."
  );

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});
