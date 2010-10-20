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
 * The Original Code is Places test code.
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

// This test makes sure that the Forget This Site command is hidden for multiple
// selections.

function test() {
  // initialization
  let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  waitForExplicitFinish();

  // Add a history entry.
  let TEST_URIs = ["http://www.mozilla.org/test1", "http://www.mozilla.org/test2"];
  ok(PlacesUtils, "checking PlacesUtils, running in chrome context?");
  let history = PlacesUtils.history;
  TEST_URIs.forEach(function(TEST_URI) {
    let visitId = history.addVisit(PlacesUtils._uri(TEST_URI), Date.now() * 1000,
                                   null, PlacesUtils.history.TRANSITION_TYPED, false, 0);
    ok(visitId > 0, TEST_URI + " successfully marked visited");
  });

  function testForgetThisSiteVisibility(selectionCount, funcNext) {
    function observer(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened")
        return;
      ww.unregisterNotification(observer);
      let organizer = aSubject.QueryInterface(Ci.nsIDOMWindow);
      SimpleTest.waitForFocus(function() {
          // Select History in the left pane.
          organizer.PlacesOrganizer.selectLeftPaneQuery('History');
          let PO = organizer.PlacesOrganizer;
          let histContainer = PO._places.selectedNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
          histContainer.containerOpen = true;
          PO._places.selectNode(histContainer.getChild(0));
          // Select the first history entry.
          let doc = organizer.document;
          let tree = PO._content;
          let selection = tree.view.selection;
          selection.clearSelection();
          selection.rangedSelect(0, selectionCount - 1, true);
          is(selection.count, selectionCount,
            "The selected range is as big as expected");
          // Open the context menu
          let contextmenu = doc.getElementById("placesContext");
          contextmenu.addEventListener("popupshown", function() {
            contextmenu.removeEventListener("popupshown", arguments.callee, false);
            let forgetThisSite = doc.getElementById("placesContext_deleteHost");
            let hideForgetThisSite = (selectionCount != 1);
            is(forgetThisSite.hidden, hideForgetThisSite,
              "The Forget this site menu item should " + (hideForgetThisSite ? "" : "not ") +
              "be hidden with " + selectionCount + " items selected");
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

    ww.registerNotification(observer);
    ww.openWindow(null,
                  "chrome://browser/content/places/places.xul",
                  "",
                  "chrome,toolbar=yes,dialog=no,resizable",
                  null);
  }

  testForgetThisSiteVisibility(1, function() {
    testForgetThisSiteVisibility(2, function() {
      // Cleanup
      history.QueryInterface(Ci.nsIBrowserHistory)
             .removeAllPages();
      finish();
    });
  });
}
