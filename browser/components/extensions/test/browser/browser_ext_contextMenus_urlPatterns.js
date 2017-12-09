/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["contextMenus"],
    },

    background: function() {
      // Test menu items using targetUrlPatterns.
      browser.contextMenus.create({
        title: "targetUrlPatterns-patternMatches-contextAll",
        targetUrlPatterns: ["*://*/*ctxmenu-image.png", "*://*/*some-link"],
        contexts: ["all"],
      });

      browser.contextMenus.create({
        title: "targetUrlPatterns-patternMatches-contextImage",
        targetUrlPatterns: ["*://*/*ctxmenu-image.png"],
        contexts: ["image"],
      });

      browser.contextMenus.create({
        title: "targetUrlPatterns-patternMatches-contextLink",
        targetUrlPatterns: ["*://*/*some-link"],
        contexts: ["link"],
      });

      browser.contextMenus.create({
        title: "targetUrlPatterns-patternDoesNotMatch-contextAll",
        targetUrlPatterns: ["*://*/does-not-match"],
        contexts: ["all"],
      });

      browser.contextMenus.create({
        title: "targetUrlPatterns-patternDoesNotMatch-contextImage",
        targetUrlPatterns: ["*://*/does-not-match"],
        contexts: ["image"],
      });

      browser.contextMenus.create({
        title: "targetUrlPatterns-patternDoesNotMatch-contextLink",
        targetUrlPatterns: ["*://*/does-not-match"],
        contexts: ["link"],
      });

      // Test menu items using documentUrlPatterns.
      browser.contextMenus.create({
        title: "documentUrlPatterns-patternMatches-contextAll",
        documentUrlPatterns: ["*://*/*context*.html"],
        contexts: ["all"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternMatches-contextFrame",
        documentUrlPatterns: ["*://*/*context_frame.html"],
        contexts: ["frame"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternMatches-contextImage",
        documentUrlPatterns: ["*://*/*context.html", "http://*/url-that-does-not-match"],
        contexts: ["image"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternMatches-contextLink",
        documentUrlPatterns: ["*://*/*context.html", "*://*/does-not-match"],
        contexts: ["link"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternDoesNotMatch-contextAll",
        documentUrlPatterns: ["*://*/does-not-match"],
        contexts: ["all"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternDoesNotMatch-contextImage",
        documentUrlPatterns: ["*://*/does-not-match"],
        contexts: ["image"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternDoesNotMatch-contextLink",
        documentUrlPatterns: ["*://*/does-not-match"],
        contexts: ["link"],
      });

      // Test menu items using both targetUrlPatterns and documentUrlPatterns.
      browser.contextMenus.create({
        title: "documentUrlPatterns-patternMatches-targetUrlPatterns-patternMatches-contextAll",
        documentUrlPatterns: ["*://*/*context.html"],
        targetUrlPatterns: ["*://*/*ctxmenu-image.png"],
        contexts: ["all"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternMatches-contextAll",
        documentUrlPatterns: ["*://*/does-not-match"],
        targetUrlPatterns: ["*://*/*ctxmenu-image.png"],
        contexts: ["all"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternMatches-targetUrlPatterns-patternDoesNotMatch-contextAll",
        documentUrlPatterns: ["*://*/*context.html"],
        targetUrlPatterns: ["*://*/does-not-match"],
        contexts: ["all"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternDoesNotMatch-contextAll",
        documentUrlPatterns: ["*://*/does-not-match"],
        targetUrlPatterns: ["*://*/does-not-match"],
        contexts: ["all"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternMatches-targetUrlPatterns-patternMatches-contextImage",
        documentUrlPatterns: ["*://*/*context.html"],
        targetUrlPatterns: ["*://*/*ctxmenu-image.png"],
        contexts: ["image"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternMatches-contextImage",
        documentUrlPatterns: ["*://*/does-not-match"],
        targetUrlPatterns: ["*://*/*ctxmenu-image.png"],
        contexts: ["image"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternMatches-targetUrlPatterns-patternDoesNotMatch-contextImage",
        documentUrlPatterns: ["*://*/*context.html"],
        targetUrlPatterns: ["*://*/does-not-match"],
        contexts: ["image"],
      });

      browser.contextMenus.create({
        title: "documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternDoesNotMatch-contextImage",
        documentUrlPatterns: ["*://*/does-not-match/"],
        targetUrlPatterns: ["*://*/does-not-match"],
        contexts: ["image"],
      });

      browser.test.notifyPass("contextmenus-urlPatterns");
    },
  });

  function confirmContextMenuItems(menu, expected) {
    for (let [label, shouldShow] of expected) {
      let items = menu.getElementsByAttribute("label", label);
      if (shouldShow) {
        is(items.length, 1, `The menu item for label ${label} was correctly shown`);
      } else {
        is(items.length, 0, `The menu item for label ${label} was correctly not shown`);
      }
    }
  }

  await extension.startup();
  await extension.awaitFinish("contextmenus-urlPatterns");

  let extensionContextMenu = await openExtensionContextMenu("#img1");
  let expected = [
    ["targetUrlPatterns-patternMatches-contextAll", true],
    ["targetUrlPatterns-patternMatches-contextImage", true],
    ["targetUrlPatterns-patternMatches-contextLink", false],
    ["targetUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["targetUrlPatterns-patternDoesNotMatch-contextImage", false],
    ["targetUrlPatterns-patternDoesNotMatch-contextLink", false],
    ["documentUrlPatterns-patternMatches-contextAll", true],
    ["documentUrlPatterns-patternMatches-contextImage", true],
    ["documentUrlPatterns-patternMatches-contextLink", false],
    ["documentUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["documentUrlPatterns-patternDoesNotMatch-contextImage", false],
    ["documentUrlPatterns-patternDoesNotMatch-contextLink", false],
    ["documentUrlPatterns-patternMatches-targetUrlPatterns-patternMatches-contextAll", true],
    ["documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternMatches-contextAll", false],
    ["documentUrlPatterns-patternMatches-targetUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["documentUrlPatterns-patternMatches-targetUrlPatterns-patternMatches-contextImage", true],
    ["documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternMatches-contextImage", false],
    ["documentUrlPatterns-patternMatches-targetUrlPatterns-patternDoesNotMatch-contextImage", false],
    ["documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternDoesNotMatch-contextImage", false],
  ];
  await confirmContextMenuItems(extensionContextMenu, expected);
  await closeContextMenu();

  let contextMenu = await openContextMenu("body");
  expected = [
    ["targetUrlPatterns-patternMatches-contextAll", false],
    ["targetUrlPatterns-patternMatches-contextImage", false],
    ["targetUrlPatterns-patternMatches-contextLink", false],
    ["targetUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["targetUrlPatterns-patternDoesNotMatch-contextImage", false],
    ["targetUrlPatterns-patternDoesNotMatch-contextLink", false],
    ["documentUrlPatterns-patternMatches-contextAll", true],
    ["documentUrlPatterns-patternMatches-contextImage", false],
    ["documentUrlPatterns-patternMatches-contextLink", false],
    ["documentUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["documentUrlPatterns-patternDoesNotMatch-contextImage", false],
    ["documentUrlPatterns-patternDoesNotMatch-contextLink", false],
    ["documentUrlPatterns-patternMatches-targetUrlPatterns-patternMatches-contextAll", false],
    ["documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternMatches-contextAll", false],
    ["documentUrlPatterns-patternMatches-targetUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["documentUrlPatterns-patternMatches-targetUrlPatterns-patternMatches-contextImage", false],
    ["documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternMatches-contextImage", false],
    ["documentUrlPatterns-patternMatches-targetUrlPatterns-patternDoesNotMatch-contextImage", false],
    ["documentUrlPatterns-patternDoesNotMatch-targetUrlPatterns-patternDoesNotMatch-contextImage", false],
  ];
  await confirmContextMenuItems(contextMenu, expected);
  await closeContextMenu();

  contextMenu = await openContextMenu("#link1");
  expected = [
    ["targetUrlPatterns-patternMatches-contextAll", true],
    ["targetUrlPatterns-patternMatches-contextImage", false],
    ["targetUrlPatterns-patternMatches-contextLink", true],
    ["targetUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["targetUrlPatterns-patternDoesNotMatch-contextImage", false],
    ["targetUrlPatterns-patternDoesNotMatch-contextLink", false],
    ["documentUrlPatterns-patternMatches-contextAll", true],
    ["documentUrlPatterns-patternMatches-contextImage", false],
    ["documentUrlPatterns-patternMatches-contextLink", true],
    ["documentUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["documentUrlPatterns-patternDoesNotMatch-contextImage", false],
    ["documentUrlPatterns-patternDoesNotMatch-contextLink", false],
  ];
  await confirmContextMenuItems(contextMenu, expected);
  await closeContextMenu();

  contextMenu = await openContextMenu("#img-wrapped-in-link");
  expected = [
    ["targetUrlPatterns-patternMatches-contextAll", true],
    ["targetUrlPatterns-patternMatches-contextImage", true],
    ["targetUrlPatterns-patternMatches-contextLink", true],
    ["targetUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["targetUrlPatterns-patternDoesNotMatch-contextImage", false],
    ["targetUrlPatterns-patternDoesNotMatch-contextLink", false],
    ["documentUrlPatterns-patternMatches-contextAll", true],
    ["documentUrlPatterns-patternMatches-contextImage", true],
    ["documentUrlPatterns-patternMatches-contextLink", true],
    ["documentUrlPatterns-patternDoesNotMatch-contextAll", false],
    ["documentUrlPatterns-patternDoesNotMatch-contextImage", false],
    ["documentUrlPatterns-patternDoesNotMatch-contextLink", false],
  ];
  await confirmContextMenuItems(contextMenu, expected);
  await closeContextMenu();

  contextMenu = await openContextMenuInFrame("frame");
  expected = [
    ["documentUrlPatterns-patternMatches-contextAll", true],
    ["documentUrlPatterns-patternMatches-contextFrame", true],
  ];
  await confirmContextMenuItems(contextMenu, expected);
  await closeContextMenu();

  await extension.unload();
  await BrowserTestUtils.removeTab(tab1);
});
