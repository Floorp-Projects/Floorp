/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URIs = [
  "http://www.mozilla.org/test1",
  "http://www.mozilla.org/test2"
];

// This test makes sure that the Forget This Site command is hidden for multiple
// selections.
add_task(function* () {
  // Add a history entry.
  ok(PlacesUtils, "checking PlacesUtils, running in chrome context?");

  let places = [];
  let transition = PlacesUtils.history.TRANSITION_TYPED;
  TEST_URIs.forEach(uri => places.push({uri: PlacesUtils._uri(uri), transition}));

  yield PlacesTestUtils.addVisits(places);
  yield testForgetThisSiteVisibility(1);
  yield testForgetThisSiteVisibility(2);

  // Cleanup.
  yield PlacesTestUtils.clearHistory();
});

var testForgetThisSiteVisibility = Task.async(function* (selectionCount) {
  let organizer = yield promiseLibrary();

  // Select History in the left pane.
  organizer.PlacesOrganizer.selectLeftPaneQuery("History");
  let PO = organizer.PlacesOrganizer;
  let histContainer = PO._places.selectedNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  histContainer.containerOpen = true;
  PO._places.selectNode(histContainer.getChild(0));

  // Select the first history entry.
  let doc = organizer.document;
  let tree = doc.getElementById("placeContent");
  let selection = tree.view.selection;
  selection.clearSelection();
  selection.rangedSelect(0, selectionCount - 1, true);
  is(selection.count, selectionCount, "The selected range is as big as expected");

  // Open the context menu.
  let contextmenu = doc.getElementById("placesContext");
  let popupShown = promisePopupShown(contextmenu);

  // Get cell coordinates.
  let rect = tree.treeBoxObject.getCoordsForCellItem(0, tree.columns[0], "text");
  // Initiate a context menu for the selected cell.
  EventUtils.synthesizeMouse(tree.body, rect.x + rect.width / 2, rect.y + rect.height / 2, {type: "contextmenu", button: 2}, organizer);
  yield popupShown;

  let forgetThisSite = doc.getElementById("placesContext_deleteHost");
  let hideForgetThisSite = (selectionCount != 1);
  is(forgetThisSite.hidden, hideForgetThisSite,
    `The Forget this site menu item should ${hideForgetThisSite ? "" : "not "}` +
    ` be hidden with ${selectionCount} items selected`);

  // Close the context menu.
  contextmenu.hidePopup();

  // Close the library window.
  yield promiseLibraryClosed(organizer);
});

function promisePopupShown(popup) {
  return new Promise(resolve => {
    popup.addEventListener("popupshown", function onShown() {
      popup.removeEventListener("popupshown", onShown, true);
      resolve();
    }, true);
  });
}
