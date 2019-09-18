/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the bookmarks Properties dialog.
 */

// DOM ids of Places sidebar trees.
const SIDEBAR_HISTORY_TREE_ID = "historyTree";
const SIDEBAR_BOOKMARKS_TREE_ID = "bookmarks-view";

const SIDEBAR_HISTORY_ID = "viewHistorySidebar";
const SIDEBAR_BOOKMARKS_ID = "viewBookmarksSidebar";

// For history sidebar.
const SIDEBAR_HISTORY_BYLASTVISITED_VIEW = "bylastvisited";
const SIDEBAR_HISTORY_BYMOSTVISITED_VIEW = "byvisited";
const SIDEBAR_HISTORY_BYDATE_VIEW = "byday";
const SIDEBAR_HISTORY_BYSITE_VIEW = "bysite";
const SIDEBAR_HISTORY_BYDATEANDSITE_VIEW = "bydateandsite";

// Action to execute on the current node.
const ACTION_EDIT = 0;
const ACTION_ADD = 1;

// If action is ACTION_ADD, set type to one of those, to define what do you
// want to create.
const TYPE_FOLDER = 0;
const TYPE_BOOKMARK = 1;

const TEST_URL = "http://www.example.com/";

const DIALOG_URL = "chrome://browser/content/places/bookmarkProperties.xul";
const DIALOG_URL_MINIMAL_UI =
  "chrome://browser/content/places/bookmarkProperties2.xul";

const { BrowserWindowTracker } = ChromeUtils.import(
  "resource:///modules/BrowserWindowTracker.jsm"
);
var win = BrowserWindowTracker.getTopWindow();

function add_bookmark(url) {
  return PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url,
    title: `bookmark/${url}`,
  });
}

// Each test is an obj w/ a desc property and run method.
var gTests = [];

// ------------------------------------------------------------------------------
// Bug 462662 - Pressing Enter to select tag from autocomplete closes bookmarks properties dialog

gTests.push({
  desc:
    "Bug 462662 - Pressing Enter to select tag from autocomplete closes bookmarks properties dialog",
  sidebar: SIDEBAR_BOOKMARKS_ID,
  action: ACTION_EDIT,
  itemType: null,
  window: null,
  _bookmark: null,
  _cleanShutdown: false,

  async setup() {
    // Add a bookmark in unsorted bookmarks folder.
    this._bookmark = await add_bookmark(TEST_URL);
    Assert.ok(this._bookmark, "Correctly added a bookmark");

    // Add a tag to this bookmark.
    PlacesUtils.tagging.tagURI(Services.io.newURI(TEST_URL), ["testTag"]);
    var tags = PlacesUtils.tagging.getTagsForURI(Services.io.newURI(TEST_URL));
    Assert.equal(tags[0], "testTag", "Correctly added a tag");
  },

  selectNode(tree) {
    tree.selectItems([PlacesUtils.bookmarks.unfiledGuid]);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    tree.selectItems([this._bookmark.guid]);
    Assert.equal(
      tree.selectedNode.bookmarkGuid,
      this._bookmark.guid,
      "Bookmark has been selected"
    );
  },

  async run() {
    // open tags autocomplete and press enter
    var tagsField = this.window.document.getElementById(
      "editBMPanel_tagsField"
    );
    var self = this;

    let unloadPromise = new Promise(resolve => {
      this.window.addEventListener(
        "unload",
        function(event) {
          tagsField.popup.removeEventListener(
            "popuphidden",
            popupListener,
            true
          );
          Assert.ok(
            self._cleanShutdown,
            "Dialog window should not be closed by pressing Enter on the autocomplete popup"
          );
          executeSoon(function() {
            resolve();
          });
        },
        { capture: true, once: true }
      );
    });

    var popupListener = {
      handleEvent(aEvent) {
        switch (aEvent.type) {
          case "popuphidden":
            // Everything worked fine, we can stop observing the window.
            self._cleanShutdown = true;
            self.window.document.documentElement.cancelDialog();
            break;
          case "popupshown":
            tagsField.popup.removeEventListener("popupshown", this, true);
            // In case this test fails the window will close, the test will fail
            // since we didn't set _cleanShutdown.
            let richlistbox = tagsField.popup.richlistbox;
            // Focus and select first result.
            Assert.equal(
              richlistbox.itemCount,
              1,
              "We have 1 autocomplete result"
            );
            tagsField.popup.selectedIndex = 0;
            Assert.equal(
              richlistbox.selectedItems.length,
              1,
              "We have selected a tag from the autocomplete popup"
            );
            info("About to focus the autocomplete results");
            richlistbox.focus();
            EventUtils.synthesizeKey("VK_RETURN", {}, self.window);
            break;
          default:
            Assert.ok(false, "unknown event: " + aEvent.type);
        }
      },
    };
    tagsField.popup.addEventListener("popupshown", popupListener, true);
    tagsField.popup.addEventListener("popuphidden", popupListener, true);

    // Open tags autocomplete popup.
    info("About to focus the tagsField");
    executeSoon(() => {
      tagsField.focus();
      tagsField.value = "";
      EventUtils.synthesizeKey("t", {}, this.window);
    });
    await unloadPromise;
  },

  finish() {
    SidebarUI.hide();
  },

  async cleanup() {
    // Check tags have not changed.
    var tags = PlacesUtils.tagging.getTagsForURI(Services.io.newURI(TEST_URL));
    Assert.equal(tags[0], "testTag", "Tag on node has not changed");

    // Cleanup.
    PlacesUtils.tagging.untagURI(Services.io.newURI(TEST_URL), ["testTag"]);
    await PlacesUtils.bookmarks.remove(this._bookmark);
    let bm = await PlacesUtils.bookmarks.fetch(this._bookmark.guid);
    Assert.ok(!bm, "should have been removed");
  },
});

// ------------------------------------------------------------------------------
// Bug 476020 - Pressing Esc while having the tag autocomplete open closes the bookmarks panel

gTests.push({
  desc:
    "Bug 476020 - Pressing Esc while having the tag autocomplete open closes the bookmarks panel",
  sidebar: SIDEBAR_BOOKMARKS_ID,
  action: ACTION_EDIT,
  itemType: null,
  window: null,
  _bookmark: null,
  _cleanShutdown: false,

  async setup() {
    // Add a bookmark in unsorted bookmarks folder.
    this._bookmark = await add_bookmark(TEST_URL);
    Assert.ok(this._bookmark, "Correctly added a bookmark");

    // Add a tag to this bookmark.
    PlacesUtils.tagging.tagURI(Services.io.newURI(TEST_URL), ["testTag"]);
    var tags = PlacesUtils.tagging.getTagsForURI(Services.io.newURI(TEST_URL));
    Assert.equal(tags[0], "testTag", "Correctly added a tag");
  },

  selectNode(tree) {
    tree.selectItems([PlacesUtils.bookmarks.unfiledGuid]);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    tree.selectItems([this._bookmark.guid]);
    Assert.equal(
      tree.selectedNode.bookmarkGuid,
      this._bookmark.guid,
      "Bookmark has been selected"
    );
  },

  async run() {
    // open tags autocomplete and press enter
    var tagsField = this.window.document.getElementById(
      "editBMPanel_tagsField"
    );
    var self = this;

    let hiddenPromise = new Promise(resolve => {
      this.window.addEventListener(
        "unload",
        function(event) {
          tagsField.popup.removeEventListener(
            "popuphidden",
            popupListener,
            true
          );
          Assert.ok(
            self._cleanShutdown,
            "Dialog window should not be closed by pressing Escape on the autocomplete popup"
          );
          executeSoon(function() {
            resolve();
          });
        },
        { capture: true, once: true }
      );
    });

    var popupListener = {
      handleEvent(aEvent) {
        switch (aEvent.type) {
          case "popuphidden":
            // Everything worked fine.
            self._cleanShutdown = true;
            self.window.document.documentElement.cancelDialog();
            break;
          case "popupshown":
            tagsField.popup.removeEventListener("popupshown", this, true);
            // In case this test fails the window will close, the test will fail
            // since we didn't set _cleanShutdown.
            let richlistbox = tagsField.popup.richlistbox;
            // Focus and select first result.
            Assert.equal(
              richlistbox.itemCount,
              1,
              "We have 1 autocomplete result"
            );
            tagsField.popup.selectedIndex = 0;
            Assert.equal(
              richlistbox.selectedItems.length,
              1,
              "We have selected a tag from the autocomplete popup"
            );
            info("About to focus the autocomplete results");
            richlistbox.focus();
            EventUtils.synthesizeKey("VK_ESCAPE", {}, self.window);
            break;
          default:
            Assert.ok(false, "unknown event: " + aEvent.type);
        }
      },
    };
    tagsField.popup.addEventListener("popupshown", popupListener, true);
    tagsField.popup.addEventListener("popuphidden", popupListener, true);

    // Open tags autocomplete popup.
    info("About to focus the tagsField");
    tagsField.focus();
    tagsField.value = "";
    EventUtils.synthesizeKey("t", {}, this.window);
    await hiddenPromise;
  },

  finish() {
    SidebarUI.hide();
  },

  async cleanup() {
    // Check tags have not changed.
    var tags = PlacesUtils.tagging.getTagsForURI(Services.io.newURI(TEST_URL));
    Assert.equal(tags[0], "testTag", "Tag on node has not changed");

    // Cleanup.
    PlacesUtils.tagging.untagURI(Services.io.newURI(TEST_URL), ["testTag"]);
    await PlacesUtils.bookmarks.remove(this._bookmark);
    let bm = await PlacesUtils.bookmarks.fetch(this._bookmark.guid);
    Assert.ok(!bm, "should have been removed");
  },
});

// ------------------------------------------------------------------------------
//  Bug 491269 - Test that editing folder name in bookmarks properties dialog does not accept the dialog

gTests.push({
  desc:
    " Bug 491269 - Test that editing folder name in bookmarks properties dialog does not accept the dialog",
  sidebar: SIDEBAR_HISTORY_ID,
  action: ACTION_ADD,
  historyView: SIDEBAR_HISTORY_BYLASTVISITED_VIEW,
  window: null,

  async setup() {
    // Add a visit.
    await PlacesTestUtils.addVisits(TEST_URL);

    this._addObserver = PlacesTestUtils.waitForNotification(
      "bookmark-added",
      null,
      "places"
    );
  },

  selectNode(tree) {
    var visitNode = tree.view.nodeForTreeIndex(0);
    tree.selectNode(visitNode);
    Assert.equal(
      tree.selectedNode.uri,
      TEST_URL,
      "The correct visit has been selected"
    );
    Assert.equal(
      tree.selectedNode.itemId,
      -1,
      "The selected node is not bookmarked"
    );
  },

  async run() {
    // Open folder selector.
    var foldersExpander = this.window.document.getElementById(
      "editBMPanel_foldersExpander"
    );
    var folderTree = this.window.gEditItemOverlay._folderTree;
    var self = this;

    let unloadPromise = new Promise(resolve => {
      this.window.addEventListener(
        "unload",
        event => {
          Assert.ok(
            self._cleanShutdown,
            "Dialog window should not be closed by pressing ESC in folder name textbox"
          );
          executeSoon(() => {
            resolve();
          });
        },
        { capture: true, once: true }
      );
    });

    folderTree.addEventListener("DOMAttrModified", function onDOMAttrModified(
      event
    ) {
      if (event.attrName != "place") {
        return;
      }
      folderTree.removeEventListener("DOMAttrModified", onDOMAttrModified);
      executeSoon(async function() {
        await self._addObserver;
        let bookmark = await PlacesUtils.bookmarks.fetch({ url: TEST_URL });
        self._bookmarkGuid = bookmark.guid;

        // Create a new folder.
        var newFolderButton = self.window.document.getElementById(
          "editBMPanel_newFolderButton"
        );
        newFolderButton.doCommand();

        // Wait for the folder to be created and for editing to start.
        await BrowserTestUtils.waitForCondition(
          () => folderTree.hasAttribute("editing"),
          "We are editing new folder name in folder tree"
        );

        // Press Escape to discard editing new folder name.
        EventUtils.synthesizeKey("VK_ESCAPE", {}, self.window);
        Assert.ok(
          !folderTree.hasAttribute("editing"),
          "We have finished editing folder name in folder tree"
        );

        self._cleanShutdown = true;
        self._removeObserver = PlacesTestUtils.waitForNotification(
          "onItemRemoved",
          (itemId, parentId, index, type, uri, guid) =>
            guid == self._bookmarkGuid
        );

        self.window.document.documentElement.cancelDialog();
      });
    });
    foldersExpander.doCommand();
    await unloadPromise;
  },

  finish() {
    SidebarUI.hide();
  },

  async cleanup() {
    await this._removeObserver;
    delete this._removeObserver;
    await PlacesTestUtils.promiseAsyncUpdates();

    await PlacesUtils.history.clear();
  },
});

// ------------------------------------------------------------------------------

add_task(async function test_setup() {
  // This test can take some time, if we timeout too early it could run
  // in the middle of other tests, or hang them.
  requestLongerTimeout(2);
});

add_task(async function test_run() {
  for (let test of gTests) {
    info(`Start of test: ${test.desc}`);
    await test.setup();

    await execute_test_in_sidebar(test);
    await test.run();

    await test.cleanup();
    await test.finish();

    info(`End of test: ${test.desc}`);
  }
});

/**
 * Global functions to run a test in Properties dialog context.
 */

function execute_test_in_sidebar(test) {
  return new Promise(resolve => {
    var sidebar = document.getElementById("sidebar");
    sidebar.addEventListener(
      "load",
      function() {
        // Need to executeSoon since the tree is initialized on sidebar load.
        executeSoon(async () => {
          await open_properties_dialog(test);
          resolve();
        });
      },
      { capture: true, once: true }
    );
    SidebarUI.show(test.sidebar);
  });
}

async function open_properties_dialog(test) {
  var sidebar = document.getElementById("sidebar");

  // If this is history sidebar, set the required view.
  if (test.sidebar == SIDEBAR_HISTORY_ID) {
    sidebar.contentDocument.getElementById(test.historyView).doCommand();
  }

  // Get sidebar's Places tree.
  var sidebarTreeID =
    test.sidebar == SIDEBAR_BOOKMARKS_ID
      ? SIDEBAR_BOOKMARKS_TREE_ID
      : SIDEBAR_HISTORY_TREE_ID;
  var tree = sidebar.contentDocument.getElementById(sidebarTreeID);
  // The sidebar may take a moment to open from the doCommand, therefore wait
  // until it has opened before continuing.
  await BrowserTestUtils.waitForCondition(
    () => tree,
    "Sidebar tree has been loaded"
  );

  // Ask current test to select the node to edit.
  test.selectNode(tree);
  Assert.ok(
    tree.selectedNode,
    "We have a places node selected: " + tree.selectedNode.title
  );

  return new Promise(resolve => {
    // Wait for the Properties dialog.
    function windowObserver(observerWindow, aTopic, aData) {
      if (aTopic != "domwindowopened") {
        return;
      }
      Services.ww.unregisterNotification(windowObserver);
      waitForFocus(async () => {
        // Ensure overlay is loaded
        await BrowserTestUtils.waitForCondition(
          () => observerWindow.gEditItemOverlay.initialized,
          "EditItemOverlay is initialized"
        );
        test.window = observerWindow;
        try {
          executeSoon(resolve);
        } catch (ex) {
          Assert.ok(false, "An error occured during test run: " + ex.message);
        }
      }, observerWindow);
    }
    Services.ww.registerNotification(windowObserver);

    var command = null;
    switch (test.action) {
      case ACTION_EDIT:
        command = "placesCmd_show:info";
        break;
      case ACTION_ADD:
        if (test.sidebar == SIDEBAR_BOOKMARKS_ID) {
          if (test.itemType == TYPE_FOLDER) {
            command = "placesCmd_new:folder";
          } else if (test.itemType == TYPE_BOOKMARK) {
            command = "placesCmd_new:bookmark";
          } else {
            Assert.ok(
              false,
              "You didn't set a valid itemType for adding an item"
            );
          }
        } else {
          command = "placesCmd_createBookmark";
        }
        break;
      default:
        Assert.ok(false, "You didn't set a valid action for this test");
    }
    // Ensure command is enabled for this node.
    Assert.ok(
      tree.controller.isCommandEnabled(command),
      " command '" + command + "' on current selected node is enabled"
    );

    // This will open the dialog. For some reason this needs to be executed
    // later, as otherwise opening the dialog throws an exception.
    executeSoon(() => {
      tree.controller.doCommand(command);
    });
  });
}
