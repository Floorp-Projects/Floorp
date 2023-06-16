/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Properties dialog on a folder.

add_task(async function test_bookmark_properties_dialog_on_folder() {
  info("Bug 479348 - Properties on a root should be read-only.");

  let bm = await PlacesUtils.bookmarks.insert({
    title: "folder",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.remove(bm);
  });

  await withSidebarTree("bookmarks", async function (tree) {
    // Select the new bookmark in the sidebar.
    tree.selectItems([bm.guid]);
    let folder = tree.selectedNode;
    Assert.equal(folder.title, "folder", "Folder title is correct");
    Assert.ok(
      tree.controller.isCommandEnabled("placesCmd_show:info"),
      "'placesCmd_show:info' on folder is enabled"
    );

    await withBookmarksDialog(
      false,
      function openDialog() {
        tree.controller.doCommand("placesCmd_show:info");
      },
      async function test(dialogWin) {
        // Check that the dialog is not read-only.
        Assert.ok(
          !dialogWin.gEditItemOverlay.readOnly,
          "EditBookmark: Dialog should not be read-only"
        );

        // Check that name picker is not read only
        let namepicker = dialogWin.document.getElementById(
          "editBMPanel_namePicker"
        );
        Assert.ok(
          !namepicker.readOnly,
          "EditBookmark: Name field should not be read-only"
        );
        Assert.equal(
          namepicker.value,
          "folder",
          "EditBookmark:Node title is correct"
        );

        fillBookmarkTextField("editBMPanel_namePicker", "newname", dialogWin);
        namepicker.blur();

        Assert.equal(
          namepicker.value,
          "newname",
          "EditBookmark: The title field has been changed"
        );

        // Confirm and close the dialog.
        namepicker.focus();
        EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
        // Ensure that the edit is finished before we hit cancel.
      }
    );

    Assert.equal(
      tree.selectedNode.title,
      "newname",
      "EditBookmark: The node has the correct title"
    );
  });
});
