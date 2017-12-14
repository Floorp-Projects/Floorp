/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the items in the bookmarks and history sidebar
// panels are clickable in both LTR and RTL modes.

var sidebar;

add_task(async function test_sidebarpanels_click() {
  ignoreAllUncaughtExceptions();

  const BOOKMARKS_SIDEBAR_ID = "viewBookmarksSidebar";
  const BOOKMARKS_SIDEBAR_TREE_ID = "bookmarks-view";
  const HISTORY_SIDEBAR_ID = "viewHistorySidebar";
  const HISTORY_SIDEBAR_TREE_ID = "historyTree";
  const TEST_URL = "http://mochi.test:8888/browser/browser/components/places/tests/browser/sidebarpanels_click_test_page.html";

  // If a sidebar is already open, close it.
  if (!document.getElementById("sidebar-box").hidden) {
    ok(false, "Unexpected sidebar found - a previous test failed to cleanup correctly");
    SidebarUI.hide();
  }

  // Ensure history is clean before starting the test.
  await PlacesUtils.history.clear();

  sidebar = document.getElementById("sidebar");
  let tests = [];

  tests.push({
    _bookmark: null,
    async init() {
      // Add a bookmark to the Unfiled Bookmarks folder.
      this._bookmark = await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        title: "test",
        url: TEST_URL,
      });

    },
    prepare() {
    },
    async selectNode(tree) {
      tree.selectItems([this._bookmark.guid]);
    },
    cleanup(aCallback) {
      return PlacesUtils.bookmarks.remove(this._bookmark);
    },
    sidebarName: BOOKMARKS_SIDEBAR_ID,
    treeName: BOOKMARKS_SIDEBAR_TREE_ID,
    desc: "Bookmarks sidebar test"
  });

  tests.push({
    async init() {
      // Add a history entry.
      let uri = PlacesUtils._uri(TEST_URL);
      await PlacesTestUtils.addVisits({
        uri, visitDate: Date.now() * 1000,
        transition: PlacesUtils.history.TRANSITION_TYPED
      });
    },
    prepare() {
      sidebar.contentDocument.getElementById("byvisited").doCommand();
    },
    selectNode(tree) {
      tree.selectNode(tree.view.nodeForTreeIndex(0));
      is(tree.selectedNode.uri, TEST_URL, "The correct visit has been selected");
      is(tree.selectedNode.itemId, -1, "The selected node is not bookmarked");
    },
    cleanup(aCallback) {
      return PlacesTestUtils.clearHistory();
    },
    sidebarName: HISTORY_SIDEBAR_ID,
    treeName: HISTORY_SIDEBAR_TREE_ID,
    desc: "History sidebar test"
  });

  for (let test of tests) {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
    await testPlacesPanel(test, () => {
      changeSidebarDirection("ltr");
      info("Running " + test.desc + " in LTR mode");
    });

    await testPlacesPanel(test, () => {
      changeSidebarDirection("rtl");
      info("Running " + test.desc + " in RTL mode");
    });

    // Remove tabs created by sub-tests.
    while (gBrowser.tabs.length > 1) {
      gBrowser.removeTab(gBrowser.tabContainer.lastChild);
    }
  }
});

async function testPlacesPanel(testInfo, preFunc) {
  await testInfo.init();

  let promise = new Promise(resolve => {
    sidebar.addEventListener("load", function() {
      executeSoon(async function() {
        testInfo.prepare();

        preFunc();

        let tree = sidebar.contentDocument.getElementById(testInfo.treeName);

        // Select the inserted places item.
        await testInfo.selectNode(tree);

        let promiseAlert = promiseAlertDialogObserved();

        synthesizeClickOnSelectedTreeCell(tree);
        // Now, wait for the observer to catch the alert dialog.
        // If something goes wrong, the test will time out at this stage.
        // Note that for the history sidebar, the URL itself is not opened,
        // and Places will show the load-js-data-url-error prompt as an alert
        // box, which means that the click actually worked, so it's good enough
        // for the purpose of this test.

        await promiseAlert;

        executeSoon(async function() {
          SidebarUI.hide();
          await testInfo.cleanup();
          resolve();
        });
      });
    }, {capture: true, once: true});
  });

  SidebarUI.show(testInfo.sidebarName);

  return promise;
}

function promiseAlertDialogObserved() {
  return new Promise(resolve => {
    function observer(subject) {
      info("alert dialog observed as expected");
      Services.obs.removeObserver(observer, "common-dialog-loaded");
      Services.obs.removeObserver(observer, "tabmodal-dialog-loaded");

      subject.Dialog.ui.button0.click();
      resolve();
    }
    Services.obs.addObserver(observer, "common-dialog-loaded");
    Services.obs.addObserver(observer, "tabmodal-dialog-loaded");
  });
}

function changeSidebarDirection(aDirection) {
  sidebar.contentDocument.documentElement.style.direction = aDirection;
}
