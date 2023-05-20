"use strict";

add_task(async function test_dialog() {
  info("Bug 479348 - Properties dialog on a root should be read-only.");
  await withSidebarTree("bookmarks", async function (tree) {
    tree.selectItems([PlacesUtils.bookmarks.unfiledGuid]);
    Assert.ok(
      !tree.controller.isCommandEnabled("placesCmd_show:info"),
      "'placesCmd_show:info' on current selected node is disabled"
    );

    await withBookmarksDialog(
      true,
      function openDialog() {
        // Even if the cmd is disabled, we can execute it regardless.
        tree.controller.doCommand("placesCmd_show:info");
      },
      async function test(dialogWin) {
        // Check that the dialog is read-only.
        Assert.ok(dialogWin.gEditItemOverlay.readOnly, "Dialog is read-only");
        // Check that accept button is disabled
        let acceptButton = dialogWin.document
          .getElementById("bookmarkpropertiesdialog")
          .getButton("accept");
        Assert.ok(acceptButton.disabled, "Accept button is disabled");

        // Check that name picker is read only
        let namepicker = dialogWin.document.getElementById(
          "editBMPanel_namePicker"
        );
        Assert.ok(namepicker.readOnly, "Name field is read-only");
        Assert.equal(
          namepicker.value,
          PlacesUtils.getString("OtherBookmarksFolderTitle"),
          "Node title is correct"
        );
      }
    );
  });
});

add_task(async function test_library() {
  info("Bug 479348 - Library info pane on a root should be read-only.");
  let library = await promiseLibrary("UnfiledBookmarks");
  registerCleanupFunction(async function () {
    await promiseLibraryClosed(library);
  });
  let PlacesOrganizer = library.PlacesOrganizer;
  let tree = PlacesOrganizer._places;
  tree.focus();
  Assert.ok(
    !tree.controller.isCommandEnabled("placesCmd_show:info"),
    "'placesCmd_show:info' on current selected node is disabled"
  );

  // Check that the pane is read-only.
  Assert.ok(library.gEditItemOverlay.readOnly, "Info pane is read-only");

  // Check that name picker is read only
  let namepicker = library.document.getElementById("editBMPanel_namePicker");
  Assert.ok(namepicker.readOnly, "Name field is read-only");
  Assert.equal(
    namepicker.value,
    PlacesUtils.getString("OtherBookmarksFolderTitle"),
    "Node title is correct"
  );
});
