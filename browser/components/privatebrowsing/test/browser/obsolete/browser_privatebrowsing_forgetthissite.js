/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the Forget This Site command is hidden in private
// browsing mode.

/**
 * Clears history invoking callback when done.
 */
function waitForClearHistory(aCallback) {
  const TOPIC_EXPIRATION_FINISHED = "places-expiration-finished";
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, TOPIC_EXPIRATION_FINISHED);
      aCallback();
    }
  };
  Services.obs.addObserver(observer, TOPIC_EXPIRATION_FINISHED, false);

  let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  hs.QueryInterface(Ci.nsIBrowserHistory).removeAllPages();
}

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  waitForExplicitFinish();

  // Add a history entry.
  const TEST_URI = "http://www.mozilla.org/privatebrowsing";
  ok(PlacesUtils, "checking PlacesUtils, running in chrome context?");
  let history = PlacesUtils.history;
  let visitId = history.addVisit(PlacesUtils._uri(TEST_URI), Date.now() * 1000,
                                 null, PlacesUtils.history.TRANSITION_TYPED, false, 0);
  ok(visitId > 0, TEST_URI + " successfully marked visited");

  function testForgetThisSiteVisibility(expected, funcNext) {
    function observer(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened")
        return;

      Services.ww.unregisterNotification(observer);
      let organizer = aSubject.QueryInterface(Ci.nsIDOMWindow);
      SimpleTest.waitForFocus(function() {
        executeSoon(function() {
          // Select History in the left pane.
          let PO = organizer.PlacesOrganizer;
          PO.selectLeftPaneQuery('History');
          let histContainer = PO._places.selectedNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
          histContainer.containerOpen = true;
          PO._places.selectNode(histContainer.getChild(0));
          // Select the first history entry.
          let doc = organizer.document;
          let tree = PO._content;
          let selection = tree.view.selection;
          selection.clearSelection();
          selection.rangedSelect(0, 0, true);
          is(tree.selectedNode.uri, TEST_URI, "The correct history item has been selected");
          // Open the context menu
          let contextmenu = doc.getElementById("placesContext");
          contextmenu.addEventListener("popupshown", function() {
            contextmenu.removeEventListener("popupshown", arguments.callee, true);
            let forgetThisSite = doc.getElementById("placesContext_deleteHost");
            is(forgetThisSite.hidden, !expected,
              "The Forget This Site menu item should " + (expected ? "not " : "") + "be hidden");
            let forgetThisSiteCmd = doc.getElementById("placesCmd_deleteDataHost");
            if (forgetThisSiteCmd.disabled, !expected,
              "The Forget This Site command should " + (expected ? "not " : "") + "be disabled");
            // Close the context menu
            contextmenu.hidePopup();
            // Wait for the Organizer window to actually be closed
            function closeObserver(aSubject, aTopic, aData) {
              if (aTopic != "domwindowclosed")
                return;
              Services.ww.unregisterNotification(closeObserver);
              SimpleTest.waitForFocus(function() {
                // Proceed
                funcNext();
              });
            }
            Services.ww.registerNotification(closeObserver);
            // Close Library window.
            organizer.close();
          }, true);
          // Get cell coordinates
          var x = {}, y = {}, width = {}, height = {};
          tree.treeBoxObject.getCoordsForCellItem(0, tree.columns[0], "text",
                                                  x, y, width, height);
          // Initiate a context menu for the selected cell
          EventUtils.synthesizeMouse(tree.body, x.value + width.value / 2, y.value + height.value / 2, {type: "contextmenu"}, organizer);
        });
      }, organizer);
    }

    Services.ww.registerNotification(observer);
    Services.ww.openWindow(null,
                           "chrome://browser/content/places/places.xul",
                           "",
                           "chrome,toolbar=yes,dialog=no,resizable",
                           null);
  }

  testForgetThisSiteVisibility(true, function() {
    // Enter private browsing mode
    pb.privateBrowsingEnabled = true;
    testForgetThisSiteVisibility(false, function() {
      // Leave private browsing mode
      pb.privateBrowsingEnabled = false;
      testForgetThisSiteVisibility(true, function() {
        // Cleanup
        waitForClearHistory(finish);
      });
    });
  });
}
