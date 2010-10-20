/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This test makes sure that the Forget This Site command is hidden in private
// browsing mode.

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
            contextmenu.removeEventListener("popupshown", arguments.callee, false);
            let forgetThisSite = doc.getElementById("placesContext_deleteHost");
            is(forgetThisSite.hidden, !expected,
              "The Forget This Site menu item should " + (expected ? "not " : "") + "be hidden");
            let forgetThisSiteCmd = doc.getElementById("placesCmd_deleteDataHost");
            if (forgetThisSiteCmd.disabled, !expected,
              "The Forget This Site command should " + (expected ? "not " : "") + "be disabled");
            // Close the context menu
            contextmenu.hidePopup();
            // Close Library window.
            organizer.close();
            // Proceed
            funcNext();
          }, false);
          // Get cell coordinates
          var x = {}, y = {}, width = {}, height = {};
          tree.treeBoxObject.getCoordsForCellItem(0, tree.columns[0], "text",
                                                  x, y, width, height);
          // Initiate a context menu for the selected cell
          EventUtils.synthesizeMouse(tree.body, x + 4, y + 4, {type: "contextmenu"}, organizer);
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
        history.QueryInterface(Ci.nsIBrowserHistory)
               .removeAllPages();
        finish();
      });
    });
  });
}
