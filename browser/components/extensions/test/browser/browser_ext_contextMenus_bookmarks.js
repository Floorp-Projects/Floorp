/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/places/tests/browser/head.js",
  this
);
/* globals withSidebarTree, synthesizeClickOnSelectedTreeCell, promiseLibrary, promiseLibraryClosed */

function bookmarkContextMenuExtension() {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["contextMenus", "bookmarks"],
    },
    async background() {
      const CONTEXT_ENTRY_LABEL = "Test Context Entry ";

      browser.contextMenus.create(
        {
          title: CONTEXT_ENTRY_LABEL,
          contexts: ["bookmark"],
          onclick: info => {
            browser.test.sendMessage(`clicked`, info.bookmarkId);
          },
        },
        () => {
          browser.test.assertEq(
            browser.runtime.lastError,
            null,
            "Created context menu"
          );
          browser.test.sendMessage("created", CONTEXT_ENTRY_LABEL);
        }
      );
    },
  });
}

add_task(async function test_bookmark_sidebar_contextmenu() {
  await withSidebarTree("bookmarks", async tree => {
    let extension = bookmarkContextMenuExtension();
    await extension.startup();
    let context_entry_label = await extension.awaitMessage("created");

    const expected_bookmarkID_2_virtualID = new Map([
      ["toolbar_____", "toolbar____v"], // Bookmarks Toolbar
      ["menu________", "menu_______v"], // Bookmarks Menu
      ["unfiled_____", "unfiled____v"], // Other Bookmarks
    ]);

    for (let [
      expectedBookmarkID,
      expectedVirtualID,
    ] of expected_bookmarkID_2_virtualID) {
      info(`Testing context menu for Bookmark ID "${expectedBookmarkID}"`);
      let sidebar = window.SidebarController.browser;
      let menu = sidebar.contentDocument.getElementById("placesContext");
      tree.selectItems([expectedBookmarkID]);

      let min = {},
        max = {};
      tree.view.selection.getRangeAt(0, min, max);
      let node = tree.view.nodeForTreeIndex(min.value);
      const actualVirtualID = node.bookmarkGuid;
      Assert.equal(actualVirtualID, expectedVirtualID, "virtualIDs match");

      let shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      synthesizeClickOnSelectedTreeCell(tree, { type: "contextmenu" });
      await shown;

      let menuItem = menu.getElementsByAttribute(
        "label",
        context_entry_label
      )[0];
      closeChromeContextMenu("placesContext", menuItem, sidebar.contentWindow);

      const actualBookmarkID = await extension.awaitMessage(`clicked`);
      Assert.equal(actualBookmarkID, expectedBookmarkID, "bookmarkIDs match");
    }
    await extension.unload();
  });
});

add_task(async function test_bookmark_library_contextmenu() {
  let extension = bookmarkContextMenuExtension();
  await extension.startup();
  let context_entry_label = await extension.awaitMessage("created");

  let library = await promiseLibrary("BookmarksToolbar");
  let menu = library.document.getElementById("placesContext");
  let leftTree = library.document.getElementById("placesList");

  const treeIDs = [
    "allbms_____v",
    "history____v",
    "downloads__v",
    "tags_______v",
  ];

  for (let treeID of treeIDs) {
    info(`Testing context menu for TreeID "${treeID}"`);
    leftTree.selectItems([treeID]);

    let shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
    synthesizeClickOnSelectedTreeCell(leftTree, { type: "contextmenu" });
    await shown;

    let items = menu.getElementsByAttribute("label", context_entry_label);
    Assert.equal(items.length, 0, "no extension context entry");
    closeChromeContextMenu("placesContext", null, library);
  }
  await extension.unload();
  await promiseLibraryClosed(library);
});
