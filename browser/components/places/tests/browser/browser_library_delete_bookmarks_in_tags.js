/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Test deleting bookmarks from within tags.
 */

registerCleanupFunction(async function() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesTestUtils.clearHistory();
});

add_task(async function test_tags() {
  const uris = [
    Services.io.newURI("http://example.com/1"),
    Services.io.newURI("http://example.com/2"),
    Services.io.newURI("http://example.com/3"),
  ];

  let children = uris.map((uri, index, arr) => {
    return {
      title: `bm${index}`,
      url: uri,
    };
  });

  // Note: we insert the uris in reverse order, so that we end up with the
  // display in "logical" order of bm0 at the top, and bm2 at the bottom.
  children = children.reverse();

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children,
  });

  for (let i = 0; i < uris.length; i++) {
    PlacesUtils.tagging.tagURI(uris[i], ["test"]);
  }

  let library = await promiseLibrary();

  // Select and open the left pane "Bookmarks Toolbar" folder.
  let PO = library.PlacesOrganizer;

  PO.selectLeftPaneQuery("Tags");
  let tagsNode = PO._places.selectedNode;
  Assert.notEqual(tagsNode, null, "Should have a valid selection");
  let tagsTitle = PlacesUtils.getString("TagsFolderTitle");
  Assert.equal(tagsNode.title, tagsTitle,
               "Should have selected the Tags node");

  // Now select the tag.
  PlacesUtils.asContainer(tagsNode).containerOpen = true;
  let tag = tagsNode.getChild(0);
  PO._places.selectNode(tag);
  Assert.equal(PO._places.selectedNode.title, "test",
               "Should have selected the created tag");

  let ContentTree = library.ContentTree;

  for (let i = 0; i < uris.length; i++) {
    ContentTree.view.selectNode(ContentTree.view.result.root.getChild(0));

    Assert.equal(ContentTree.view.selectedNode.title, `bm${i}`,
                 `Should have selected bm${i}`);

    let promiseNotification = PlacesTestUtils.waitForNotification("onItemChanged",
      (id, property) => property == "tags");

    ContentTree.view.controller.doCommand("cmd_delete");

    await promiseNotification;

    for (let j = 0; j < uris.length; j++) {
      let tags = PlacesUtils.tagging.getTagsForURI(uris[j]);
      if (j <= i) {
        Assert.equal(tags.length, 0,
                     `There should be no tags for the URI: ${uris[j].spec}`);
      } else {
        Assert.equal(tags.length, 1,
                     `There should be one tag for the URI: ${uris[j].spec}`);
      }
    }
  }

  // The tag should now not exist.
  Assert.deepEqual(PlacesUtils.tagging.getURIsForTag("test"), [],
                   "There should be no URIs remaining for the tag");

  tagsNode.containerOpen = false;

  library.close();
});
