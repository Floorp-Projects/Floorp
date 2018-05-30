/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Test appropriate visibility of infoBoxExpanderWrapper and
 *  additionalInfoFields in infoBox section of library
 */

let gLibrary;
add_task(async function() {
  // Open Library.
  gLibrary = await promiseLibrary();
  registerCleanupFunction(async () => {
    gLibrary.close();
    await PlacesUtils.history.clear();
  });
  gLibrary.PlacesOrganizer._places.focus();

  info("Bug 430148 - Remove or hide the more/less button in details pane...");
  let PO = gLibrary.PlacesOrganizer;
  let ContentTree = gLibrary.ContentTree;
  let infoBoxExpanderWrapper = getAndCheckElmtById("infoBoxExpanderWrapper");

  await PlacesTestUtils.addVisits("http://www.mozilla.org/");

  // open all bookmarks node
  PO.selectLeftPaneBuiltIn("AllBookmarks");
  isnot(PO._places.selectedNode, null,
        "Correctly selected all bookmarks node.");
  checkInfoBoxSelected();
  ok(infoBoxExpanderWrapper.hidden,
      "Expander button is hidden for all bookmarks node.");
  checkAddInfoFieldsCollapsed(PO);

  // open history node
  PO.selectLeftPaneBuiltIn("History");
  isnot(PO._places.selectedNode, null, "Correctly selected history node.");
  checkInfoBoxSelected();
  ok(infoBoxExpanderWrapper.hidden,
      "Expander button is hidden for history node.");
  checkAddInfoFieldsCollapsed(PO);

  // open history child node
  var historyNode = PO._places.selectedNode.
                    QueryInterface(Ci.nsINavHistoryContainerResultNode);
  historyNode.containerOpen = true;
  var childNode = historyNode.getChild(0);
  isnot(childNode, null, "History node first child is not null.");
  PO._places.selectNode(childNode);
  checkInfoBoxSelected();
  ok(infoBoxExpanderWrapper.hidden,
      "Expander button is hidden for history child node.");
  checkAddInfoFieldsCollapsed(PO);

  // open history item
  var view = ContentTree.view.view;
  ok(view.rowCount > 0, "History item exists.");
  view.selection.select(0);
  ok(infoBoxExpanderWrapper.hidden,
      "Expander button is hidden for history item.");
  checkAddInfoFieldsCollapsed(PO);

  historyNode.containerOpen = false;

  // open bookmarks menu node
  PO.selectLeftPaneBuiltIn("BookmarksMenu");
  isnot(PO._places.selectedNode, null,
        "Correctly selected bookmarks menu node.");
  checkInfoBoxSelected();
  ok(infoBoxExpanderWrapper.hidden,
      "Expander button is hidden for bookmarks menu node.");
  checkAddInfoFieldsCollapsed(PO);

  // open recently bookmarked node
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "place:" + PlacesUtils.bookmarks.userContentRoots.map(guid => `parent=${guid}`).join("&") +
         "&queryType=" + Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS +
         "&sort=" + Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING +
         "&maxResults=10" +
         "&excludeQueries=1",
    title: "Recent Bookmarks",
    index: 0
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://mozilla.org/",
    title: "Mozilla",
    index: 1
  });
  var menuNode = PO._places.selectedNode.
                  QueryInterface(Ci.nsINavHistoryContainerResultNode);
  menuNode.containerOpen = true;
  childNode = menuNode.getChild(0);
  isnot(childNode, null, "Bookmarks menu child node exists.");
  is(childNode.title, "Recent Bookmarks",
      "Correctly selected recently bookmarked node.");
  PO._places.selectNode(childNode);
  checkInfoBoxSelected();
  ok(infoBoxExpanderWrapper.hidden,
     "Expander button is hidden for recently bookmarked node.");
  checkAddInfoFieldsCollapsed(PO);

  // open first bookmark
  view = ContentTree.view.view;
  ContentTree.view.focus();
  ok(view.rowCount > 0, "Bookmark item exists.");
  view.selection.select(0);
  checkInfoBoxSelected();
  ok(!infoBoxExpanderWrapper.hidden,
      "Expander button is not hidden for bookmark item.");
  checkAddInfoFieldsNotCollapsed(PO);

  ok(true, "Checking additional info fields visibiity for bookmark item");
  var expanderButton = getAndCheckElmtById("infoBoxExpander");

  // make sure additional fields are hidden by default
  PO._additionalInfoFields.forEach(function(id) {
    ok(getAndCheckElmtById(id).hidden,
       "Additional info field correctly hidden by default: #" + id);
  });

  // toggle fields and make sure they are hidden/unhidden as expected
  expanderButton.click();
  PO._additionalInfoFields.forEach(function(id) {
    ok(!getAndCheckElmtById(id).hidden,
       "Additional info field correctly unhidden after toggle: #" + id);
  });
  expanderButton.click();
  PO._additionalInfoFields.forEach(function(id) {
    ok(getAndCheckElmtById(id).hidden,
       "Additional info field correctly hidden after toggle: #" + id);
  });

  menuNode.containerOpen = false;
});

function checkInfoBoxSelected() {
  is(getAndCheckElmtById("detailsDeck").selectedIndex, 1,
     "Selected element in detailsDeck is infoBox.");
}

function checkAddInfoFieldsCollapsed(PO) {
  PO._additionalInfoFields.forEach(id => {
    ok(getAndCheckElmtById(id).collapsed,
       `Additional info field should be collapsed: #${id}`);
  });
}

function checkAddInfoFieldsNotCollapsed(PO) {
  ok(PO._additionalInfoFields.some(id => !getAndCheckElmtById(id).collapsed),
     `Some additional info field should not be collapsed.`);
}

function getAndCheckElmtById(id) {
  var elmt = gLibrary.document.getElementById(id);
  isnot(elmt, null, "Correctly got element: #" + id);
  return elmt;
}
