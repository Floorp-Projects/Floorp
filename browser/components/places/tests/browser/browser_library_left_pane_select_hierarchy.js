/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function() {
  let hierarchy = [ "AllBookmarks", "BookmarksMenu" ];

  let items = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      title: "Folder 1",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        title: "Folder 2",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children: [{
          title: "Bookmark",
          url: "http://example.com",
        }],
      }],
    }],
  });

  let folder1Id = await PlacesUtils.promiseItemId(items[0].guid);
  let folder2Id = await PlacesUtils.promiseItemId(items[1].guid);

  hierarchy.push(folder1Id, folder2Id);

  let library = await promiseLibrary();

  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.remove(items[0]);
    await promiseLibraryClosed(library);
  });

  library.PlacesOrganizer.selectLeftPaneContainerByHierarchy(hierarchy);

  Assert.equal(library.PlacesOrganizer._places.selectedNode.bookmarkGuid, items[1].guid,
    "Found the expected left pane selected node");

  Assert.equal(library.ContentTree.view.view.nodeForTreeIndex(0).bookmarkGuid, items[2].guid,
    "Found the expected right pane contents");
});
