"use strict";

add_task(async function() {
  info("Bug 479348 - Properties on a root should be read-only.");

  await withSidebarTree("bookmarks", async function(tree) {
    tree.selectItems([PlacesUtils.bookmarks.unfiledGuid]);
    Assert.ok(tree.controller.isCommandEnabled("placesCmd_show:info"),
              "'placesCmd_show:info' on current selected node is enabled");

    await withBookmarksDialog(
      true,
      function openDialog() {
        tree.controller.doCommand("placesCmd_show:info");
      },
      async function test(dialogWin) {
        // Check that the dialog is read-only.
        Assert.ok(dialogWin.gEditItemOverlay.readOnly, "Dialog is read-only");
        // Check that accept button is disabled
        let acceptButton = dialogWin.document.documentElement.getButton("accept");
        Assert.ok(acceptButton.disabled, "Accept button is disabled");

        // Check that name picker is read only
        let namepicker = dialogWin.document.getElementById("editBMPanel_namePicker");
        Assert.ok(namepicker.readOnly, "Name field is read-only");
        let bookmark = await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.unfiledGuid);
        Assert.equal(namepicker.value, bookmark.title, "Node title is correct");
        // Blur the field and ensure root's name has not been changed.
        namepicker.blur();
        bookmark = await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.unfiledGuid);
        Assert.equal(namepicker.value, bookmark.title, "Root title is correct");
        // Check the shortcut's title.
        info(tree.selectedNode.bookmarkGuid);
        bookmark = await PlacesUtils.bookmarks.fetch(tree.selectedNode.bookmarkGuid);
        Assert.equal(bookmark.title, "", "Shortcut title is null");
      }
    );
  });
});
