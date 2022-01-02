/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const TEST_URL = Services.io.newURI("http://example.com/");
const MOZURISPEC = Services.io.newURI("http://mozilla.com/");

add_task(async function() {
  let organizer = await promiseLibrary();

  ok(PlacesUtils, "PlacesUtils in scope");
  ok(PlacesUIUtils, "PlacesUIUtils in scope");

  let PlacesOrganizer = organizer.PlacesOrganizer;
  ok(PlacesOrganizer, "Places organizer in scope");

  let ContentTree = organizer.ContentTree;
  ok(ContentTree, "ContentTree is in scope");

  let visits = {
    uri: MOZURISPEC,
    transition: PlacesUtils.history.TRANSITION_TYPED,
  };
  await PlacesTestUtils.addVisits(visits);

  // create an initial tag to work with
  let newBookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    title: "bookmark/" + TEST_URL.spec,
    url: TEST_URL,
  });

  ok(newBookmark, "A bookmark was added");
  PlacesUtils.tagging.tagURI(TEST_URL, ["foo"]);
  let tags = PlacesUtils.tagging.getTagsForURI(TEST_URL);
  is(tags[0], "foo", "tag is foo");

  // focus the new tag
  focusTag(PlacesOrganizer);

  let populate = () => copyHistNode(PlacesOrganizer, ContentTree);
  await promiseClipboard(populate, PlacesUtils.TYPE_X_MOZ_PLACE);

  focusTag(PlacesOrganizer);
  await PlacesOrganizer._places.controller.paste();

  // re-focus the history again
  PlacesOrganizer.selectLeftPaneBuiltIn("History");
  let histContainer = PlacesOrganizer._places.selectedNode;
  PlacesUtils.asContainer(histContainer);
  histContainer.containerOpen = true;
  PlacesOrganizer._places.selectNode(histContainer.getChild(0));
  let histNode = ContentTree.view.view.nodeForTreeIndex(0);
  ok(histNode, "histNode exists: " + histNode.title);

  // check to see if the history node is tagged!
  tags = PlacesUtils.tagging.getTagsForURI(MOZURISPEC);
  ok(tags.length == 1, "history node is tagged: " + tags.length);

  // check if a bookmark was created
  let bookmarks = [];
  await PlacesUtils.bookmarks.fetch({ url: MOZURISPEC }, bm => {
    bookmarks.push(bm);
  });
  ok(!!bookmarks.length, "bookmark exists for the tagged history item");

  // is the bookmark visible in the UI?
  // get the Unsorted Bookmarks node
  PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");

  // now we can see what is in the ContentTree tree
  let unsortedNode = ContentTree.view.view.nodeForTreeIndex(1);
  ok(unsortedNode, "unsortedNode is not null: " + unsortedNode.uri);
  is(unsortedNode.uri, MOZURISPEC.spec, "node uri's are the same");

  await promiseLibraryClosed(organizer);

  // Remove new Places data we created.
  PlacesUtils.tagging.untagURI(MOZURISPEC, ["foo"]);
  PlacesUtils.tagging.untagURI(TEST_URL, ["foo"]);
  tags = PlacesUtils.tagging.getTagsForURI(TEST_URL);
  is(tags.length, 0, "tags are gone");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

function focusTag(PlacesOrganizer) {
  PlacesOrganizer.selectLeftPaneBuiltIn("Tags");
  let tags = PlacesOrganizer._places.selectedNode;
  tags.containerOpen = true;
  let fooTag = tags.getChild(0);
  let tagNode = fooTag;
  PlacesOrganizer._places.selectNode(fooTag);
  is(tagNode.title, "foo", "tagNode title is foo");
  let ip = PlacesOrganizer._places.insertionPoint;
  ok(ip.isTag, "IP is a tag");
}

function copyHistNode(PlacesOrganizer, ContentTree) {
  // focus the history object
  PlacesOrganizer.selectLeftPaneBuiltIn("History");
  let histContainer = PlacesOrganizer._places.selectedNode;
  PlacesUtils.asContainer(histContainer);
  histContainer.containerOpen = true;
  PlacesOrganizer._places.selectNode(histContainer.getChild(0));
  let histNode = ContentTree.view.view.nodeForTreeIndex(0);
  ContentTree.view.selectNode(histNode);
  is(histNode.uri, MOZURISPEC.spec, "historyNode exists: " + histNode.uri);
  // copy the history node
  ContentTree.view.controller.copy();
}
