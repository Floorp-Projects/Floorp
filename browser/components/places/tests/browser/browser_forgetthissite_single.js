/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URIs = [
  "http://www.mozilla.org/test1",
  "http://www.mozilla.org/test2",
];

// This test makes sure that the Forget This Site command is hidden for multiple
// selections.
add_task(async function() {
  // Add a history entry.
  ok(PlacesUtils, "checking PlacesUtils, running in chrome context?");

  let places = [];
  let transition = PlacesUtils.history.TRANSITION_TYPED;
  TEST_URIs.forEach(uri => places.push({uri: Services.io.newURI(uri), transition}));

  await PlacesTestUtils.addVisits(places);
  await testForgetThisSiteVisibility(1);
  await testForgetThisSiteVisibility(2);

  // Cleanup.
  await PlacesUtils.history.clear();
});

var testForgetThisSiteVisibility = async function(selectionCount) {
  let organizer = await promiseLibrary();

  // Select History in the left pane.
  organizer.PlacesOrganizer.selectLeftPaneBuiltIn("History");
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
  let rect = tree.getCoordsForCellItem(0, tree.columns[0], "text");
  // Initiate a context menu for the selected cell.
  EventUtils.synthesizeMouse(tree.body, rect.x + rect.width / 2, rect.y + rect.height / 2, {type: "contextmenu", button: 2}, organizer);
  await popupShown;

  let forgetThisSite = doc.getElementById("placesContext_deleteHost");
  let hideForgetThisSite = (selectionCount != 1);
  is(forgetThisSite.hidden, hideForgetThisSite,
    `The Forget this site menu item should ${hideForgetThisSite ? "" : "not "}` +
    ` be hidden with ${selectionCount} items selected`);

  // Close the context menu.
  contextmenu.hidePopup();

  // Close the library window.
  await promiseLibraryClosed(organizer);
};

function promisePopupShown(popup) {
  return new Promise(resolve => {
    popup.addEventListener("popupshown", function() {
      resolve();
    }, {capture: true, once: true});
  });
}
