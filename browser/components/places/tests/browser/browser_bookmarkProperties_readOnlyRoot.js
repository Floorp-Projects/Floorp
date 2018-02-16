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
        Assert.equal(namepicker.value,
          PlacesUtils.getString("OtherBookmarksFolderTitle"), "Node title is correct");
      }
    );
  });
});
