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

// This test makes sure that the items in the bookmarks and history sidebar
// panels are clickable in both LTR and RTL modes.

function test() {
  const BOOKMARKS_SIDEBAR_ID = "viewBookmarksSidebar";
  const BOOKMARKS_SIDEBAR_TREE_ID = "bookmarks-view";
  const HISTORY_SIDEBAR_ID = "viewHistorySidebar";
  const HISTORY_SIDEBAR_TREE_ID = "historyTree";

  // Initialization.
  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  let bs = PlacesUtils.bookmarks;
  let hs = PlacesUtils.history;
  let sidebarBox = document.getElementById("sidebar-box");
  let sidebar = document.getElementById("sidebar");
  waitForExplicitFinish();

  // If a sidebar is already open, close it.
  if (!sidebarBox.hidden) {
    info("Unexpected sidebar found - a previous test failed to cleanup correctly");
    toggleSidebar();
  }

  const TEST_URL = "javascript:alert(\"test\");";

  let tests = [];
  tests.push({
    _itemID: null,
    init: function() {
      // Add a bookmark to the Unfiled Bookmarks folder.
      this._itemID = bs.insertBookmark(bs.unfiledBookmarksFolder,
                                       PlacesUtils._uri(TEST_URL),
                                       bs.DEFAULT_INDEX, "test");
    },
    prepare: function() {
    },
    selectNode: function(tree) {
      tree.selectItems([this._itemID]);
    },
    cleanup: function() {
      bs.removeFolderChildren(bs.unfiledBookmarksFolder);
    },
    sidebarName: BOOKMARKS_SIDEBAR_ID,
    treeName: BOOKMARKS_SIDEBAR_TREE_ID,
    desc: "Bookmarks sidebar test"
  });

  tests.push({
    init: function() {
      // Add a history entry.
      this.cleanup();
      hs.addVisit(PlacesUtils._uri(TEST_URL), Date.now() * 1000,
                  null, hs.TRANSITION_TYPED, false, 0);
    },
    prepare: function() {
      sidebar.contentDocument.getElementById("byvisited").doCommand();
    },
    selectNode: function(tree) {
      tree.selectNode(tree.view.nodeForTreeIndex(0));
      is(tree.selectedNode.uri, TEST_URL, "The correct visit has been selected");
      is(tree.selectedNode.itemId, -1, "The selected node is not bookmarked");
    },
    cleanup: function() {
      hs.QueryInterface(Ci.nsIBrowserHistory)
        .removeAllPages();
    },
    sidebarName: HISTORY_SIDEBAR_ID,
    treeName: HISTORY_SIDEBAR_TREE_ID,
    desc: "History sidebar test"
  });

  let currentTest;

  function testPlacesPanel(preFunc, postFunc) {
    currentTest.init();

    sidebar.addEventListener("load", function() {
      sidebar.removeEventListener("load", arguments.callee, true);

      let doc = sidebar.contentDocument;
      let tree = doc.getElementById(currentTest.treeName);
      let tbo = tree.treeBoxObject;

      executeSoon(function() {
        currentTest.prepare();
        if (preFunc)
          preFunc();

        function observer(aSubject, aTopic, aData) {
          info("alert dialog observed as expected");
          os.removeObserver(observer, "common-dialog-loaded");
          os.removeObserver(observer, "tabmodal-dialog-loaded");

          aSubject.Dialog.ui.button0.click();

          executeSoon(function () {
              toggleSidebar(currentTest.sidebarName);
              currentTest.cleanup();
              postFunc();
            });
        }
        os.addObserver(observer, "common-dialog-loaded", false);
        os.addObserver(observer, "tabmodal-dialog-loaded", false);

        // Select the inserted places item.
        currentTest.selectNode(tree);
        is(tbo.view.selection.count, 1,
           "The test node should be successfully selected");
        // Get its row ID.
        let min = {}, max = {};
        tbo.view.selection.getRangeAt(0, min, max);
        let rowID = min.value;
        tbo.ensureRowIsVisible(rowID);

        // Calculate the click coordinates.
        let x = {}, y = {}, width = {}, height = {};
        tbo.getCoordsForCellItem(rowID, tree.columns[0], "text",
                                 x, y, width, height);
        x = x.value + width.value / 2;
        y = y.value + height.value / 2;
        // Simulate the click.
        EventUtils.synthesizeMouse(tree.body, x, y, {}, doc.defaultView);
        // Now, wait for the observer to catch the alert dialog.
        // If something goes wrong, the test will time out at this stage.
        // Note that for the history sidebar, the URL itself is not opened,
        // and Places will show the load-js-data-url-error prompt as an alert
        // box, which means that the click actually worked, so it's good enough
        // for the purpose of this test.
      });
    }, true);
    toggleSidebar(currentTest.sidebarName);
  }

  function changeSidebarDirection(aDirection) {
    document.getElementById("sidebar")
            .contentDocument
            .documentElement
            .style.direction = aDirection;
  }

  function runNextTest() {
    if (tests.length == 0)
      finish();
    else {
      currentTest = tests.shift();
      testPlacesPanel(function() {
        changeSidebarDirection("ltr");
        info("Running " + currentTest.desc + " in LTR mode");
      }, function() {
        executeSoon(function() {
          testPlacesPanel(function() {
            // Run the test in RTL mode.
            changeSidebarDirection("rtl");
            info("Running " + currentTest.desc + " in RTL mode");
          }, function() {
            executeSoon(runNextTest);
          });
        });
      });
    }
  }

  runNextTest();
}
