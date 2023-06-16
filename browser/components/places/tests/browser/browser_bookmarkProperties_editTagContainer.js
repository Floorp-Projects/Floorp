"use strict";

add_task(async function editTagContainer() {
  info("Bug 479348 - Properties on a root should be read-only.");
  let uri = Services.io.newURI("http://example.com/");
  let bm = await PlacesUtils.bookmarks.insert({
    url: uri.spec,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.remove(bm);
  });

  PlacesUtils.tagging.tagURI(uri, ["tag1"]);

  let library = await promiseLibrary();
  let PlacesOrganizer = library.PlacesOrganizer;
  registerCleanupFunction(async function () {
    await promiseLibraryClosed(library);
  });

  PlacesOrganizer.selectLeftPaneBuiltIn("Tags");
  let tree = PlacesOrganizer._places;
  let tagsContainer = tree.selectedNode;
  tagsContainer.containerOpen = true;
  let fooTag = tagsContainer.getChild(0);
  let tagNode = fooTag;
  tree.selectNode(fooTag);
  Assert.equal(tagNode.title, "tag1", "EditBookmark: tagNode title is correct");

  Assert.ok(
    tree.controller.isCommandEnabled("placesCmd_show:info"),
    "EditBookmark: 'placesCmd_show:info' on current selected node is enabled"
  );

  await withBookmarksDialog(
    true,
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
        "tag1",
        "EditBookmark: Node title is correct"
      );

      fillBookmarkTextField("editBMPanel_namePicker", "tag2", dialogWin);

      // Although we have received the expected notifications, we need
      // to let everything resolve to ensure the UI is updated.
      await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));

      Assert.equal(
        namepicker.value,
        "tag2",
        "EditBookmark: The title field has been changed"
      );

      // Try to set an empty title, it should restore the previous one.
      fillBookmarkTextField("editBMPanel_namePicker", "", dialogWin);

      Assert.equal(
        namepicker.value,
        "tag1",
        "EditBookmark: The title field has been changed"
      );
    }
  );

  // Check the tag change hasn't changed
  let tags = PlacesUtils.tagging.getTagsForURI(uri);
  Assert.equal(tags.length, 1, "EditBookmark: Found the right number of tags");
  Assert.deepEqual(
    tags,
    ["tag1"],
    "EditBookmark: Found the expected unchanged tag"
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
        "Dialog should not be read-only"
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
        "tag1",
        "EditBookmark: Node title is correct"
      );

      fillBookmarkTextField("editBMPanel_namePicker", "tag2", dialogWin);

      Assert.equal(
        namepicker.value,
        "tag2",
        "EditBookmark: The title field has been changed"
      );
      namepicker.blur();

      EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
    }
  );

  tags = PlacesUtils.tagging.getTagsForURI(uri);
  Assert.equal(tags.length, 1, "EditBookmark: Found the right number of tags");
  Assert.deepEqual(
    tags,
    ["tag2"],
    "EditBookmark: Found the expected Y changed tag"
  );
});
