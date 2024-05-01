/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gInsertedBookmarks;
add_setup(async function () {
  gInsertedBookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "bm1",
        url: "about:buildconfig",
      },
      {
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "folder",
        children: [
          {
            title: "bm2",
            url: "about:mozilla",
          },
        ],
      },
    ],
  });
  registerCleanupFunction(PlacesUtils.bookmarks.eraseEverything);
});

add_task(async function test_open_sidebar_and_customize() {
  await withSidebarTree("bookmarks", async tree => {
    async function checkTreeIsFunctional() {
      Assert.ok(SidebarUI.isOpen, "Sidebar is open");
      Assert.ok(
        BrowserTestUtils.isVisible(SidebarUI.browser),
        "sidebar browser is visible"
      );
      Assert.ok(tree.view.result, "View result is defined");
      await TestUtils.waitForCondition(
        () => tree.view.result.root.containerOpen,
        "View root node should be reopened"
      );
      toggleFolder(tree, gInsertedBookmarks[1].guid);
    }

    await checkTreeIsFunctional();

    info("Starting customization");
    await promiseCustomizeStart();

    Assert.ok(
      !BrowserTestUtils.isVisible(SidebarUI.browser),
      "sidebar browser is hidden"
    );
    Assert.ok(tree.view.result, "View result is defined");
    Assert.ok(!tree.view.result.root.containerOpen, "View root node is closed");

    info("Ending customization");
    await promiseCustomizeEnd();

    await checkTreeIsFunctional();
  });
});

function promiseCustomizeStart(win = window) {
  return new Promise(resolve => {
    win.gNavToolbox.addEventListener("customizationready", resolve, {
      once: true,
    });
    win.gCustomizeMode.enter();
  });
}

function promiseCustomizeEnd(win = window) {
  return new Promise(resolve => {
    win.gNavToolbox.addEventListener("aftercustomization", resolve, {
      once: true,
    });
    win.gCustomizeMode.exit();
  });
}

function toggleFolder(tree, guid) {
  tree.selectItems([guid]);
  Assert.equal(tree.selectedNode.title, "folder");
  Assert.ok(
    !PlacesUtils.asContainer(tree.selectedNode).containerOpen,
    "Folder is closed"
  );
  synthesizeClickOnSelectedTreeCell(tree);
  Assert.ok(
    PlacesUtils.asContainer(tree.selectedNode).containerOpen,
    "Folder is open"
  );
  synthesizeClickOnSelectedTreeCell(tree);
  Assert.ok(
    !PlacesUtils.asContainer(tree.selectedNode).containerOpen,
    "Folder is closed"
  );
}
