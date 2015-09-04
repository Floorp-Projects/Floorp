"use strict"

add_task(function* () {
  info("Bug 479348 - Properties on a root should be read-only.");
  let uri = NetUtil.newURI("http://example.com/");
  let bm = yield PlacesUtils.bookmarks.insert({
    url: uri.spec,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid
  });
  registerCleanupFunction(function* () {
    yield PlacesUtils.bookmarks.remove(bm);
  });

  PlacesUtils.tagging.tagURI(uri, ["tag1"]);

  let library = yield promiseLibrary();
  let PlacesOrganizer = library.PlacesOrganizer;
  registerCleanupFunction(function* () {
    yield promiseLibraryClosed(library);
  });

  PlacesOrganizer.selectLeftPaneQuery("Tags");
  let tree = PlacesOrganizer._places;
  let tagsContainer = tree.selectedNode;
  tagsContainer.containerOpen = true;
  let fooTag = tagsContainer.getChild(0);
  let tagNode = fooTag;
  tree.selectNode(fooTag);
  is(tagNode.title, 'tag1', "tagNode title is correct");

  ok(tree.controller.isCommandEnabled("placesCmd_show:info"),
     "'placesCmd_show:info' on current selected node is enabled");

  yield withBookmarksDialog(
    true,
    function openDialog() {
      tree.controller.doCommand("placesCmd_show:info");
    },
    function* test(dialogWin) {
      // Check that the dialog is not read-only.
      ok(!dialogWin.gEditItemOverlay.readOnly, "Dialog should not be read-only");

      // Check that name picker is not read only
      let namepicker = dialogWin.document.getElementById("editBMPanel_namePicker");
      ok(!namepicker.readOnly, "Name field should not be read-only");
      is(namepicker.value, "tag1", "Node title is correct");

      let promiseTitleChangeNotification = promiseBookmarksNotification(
          "onItemChanged", (itemId, prop, isAnno, val) => prop == "title" && val == "tag2");

      fillBookmarkTextField("editBMPanel_namePicker", "tag2", dialogWin);

      yield promiseTitleChangeNotification;

      is(namepicker.value, "tag2", "Node title has been properly edited");

      // Check the shortcut's title.
      is(tree.selectedNode.title, "tag2", "The node has the correct title");

      // Check the tags have been edited.
      let tags = PlacesUtils.tagging.getTagsForURI(uri);
      is(tags.length, 1, "Found the right number of tags");
      ok(tags.includes("tag2"), "Found the expected tag");
    }
  );

  // Check the tag change has been reverted.
  let tags = PlacesUtils.tagging.getTagsForURI(uri);
  is(tags.length, 1, "Found the right number of tags");
  ok(tags.includes("tag1"), "Found the expected tag");
});
