"use strict";

add_task(async function() {
  info("Bug 479348 - Properties on a root should be read-only.");
  let uri = Services.io.newURI("http://example.com/");
  let bm = await PlacesUtils.bookmarks.insert({
    url: uri.spec,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.remove(bm);
  });

  PlacesUtils.tagging.tagURI(uri, ["tag1"]);

  let library = await promiseLibrary();
  let PlacesOrganizer = library.PlacesOrganizer;
  registerCleanupFunction(async function() {
    await promiseLibraryClosed(library);
  });

  PlacesOrganizer.selectLeftPaneBuiltIn("Tags");
  let tree = PlacesOrganizer._places;
  let tagsContainer = tree.selectedNode;
  tagsContainer.containerOpen = true;
  let fooTag = tagsContainer.getChild(0);
  let tagNode = fooTag;
  tree.selectNode(fooTag);
  Assert.equal(tagNode.title, "tag1", "tagNode title is correct");

  Assert.ok(
    tree.controller.isCommandEnabled("placesCmd_show:info"),
    "'placesCmd_show:info' on current selected node is enabled"
  );

  let promiseTagResetNotification = PlacesTestUtils.waitForNotification(
    "bookmark-tags-changed",
    events =>
      events.some(({ tags }) => tags.length === 1 && tags[0] === "tag1"),
    "places"
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
        "Dialog should not be read-only"
      );

      // Check that name picker is not read only
      let namepicker = dialogWin.document.getElementById(
        "editBMPanel_namePicker"
      );
      Assert.ok(!namepicker.readOnly, "Name field should not be read-only");
      Assert.equal(namepicker.value, "tag1", "Node title is correct");
      let promiseTagChangeNotification = PlacesTestUtils.waitForNotification(
        "bookmark-tags-changed",
        events =>
          events.some(({ tags }) => tags.length === 1 && tags[0] === "tag2"),
        "places"
      );

      let promiseTagRemoveNotification = PlacesTestUtils.waitForNotification(
        "bookmark-removed",
        events =>
          events.some(
            event => event.parentGuid == PlacesUtils.bookmarks.tagsGuid
          ),
        "places"
      );
      fillBookmarkTextField("editBMPanel_namePicker", "tag2", dialogWin);

      await promiseTagChangeNotification;
      await promiseTagRemoveNotification;

      // Although we have received the expected notifications, we need
      // to let everything resolve to ensure the UI is updated.
      await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));

      Assert.equal(
        namepicker.value,
        "tag2",
        "The title field has been changed"
      );

      // Check the shortcut's title.
      Assert.equal(
        tree.selectedNode.title,
        "tag2",
        "The node has the correct title"
      );

      // Try to set an empty title, it should restore the previous one.
      fillBookmarkTextField("editBMPanel_namePicker", "", dialogWin);
      Assert.equal(
        namepicker.value,
        "tag2",
        "The title field has been changed"
      );
      Assert.equal(
        tree.selectedNode.title,
        "tag2",
        "The node has the correct title"
      );

      // Check the tags have been edited.
      let tags = PlacesUtils.tagging.getTagsForURI(uri);
      Assert.equal(tags.length, 1, "Found the right number of tags");
      Assert.ok(tags.includes("tag2"), "Found the expected tag");

      // Ensure that the addition really is finished before we hit cancel.
      await PlacesTestUtils.promiseAsyncUpdates();
    }
  );

  await promiseTagResetNotification;

  // Check the tag change has been reverted.
  let tags = PlacesUtils.tagging.getTagsForURI(uri);
  Assert.equal(tags.length, 1, "Found the right number of tags");
  Assert.deepEqual(tags, ["tag1"], "Found the expected tag");
});
