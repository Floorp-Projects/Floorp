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
const DIALOG_URL_MINIMAL_UI = "chrome://browser/content/places/bookmarkProperties2.xul";

Cu.import("resource:///modules/RecentWindow.jsm");
let win = RecentWindow.getMostRecentBrowserWindow();
var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
         getService(Ci.nsIWindowWatcher);

function add_bookmark(aURI) {
  var bId = PlacesUtils.bookmarks
                       .insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       aURI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "bookmark/" + aURI.spec);
  return bId;
}

// Each test is an obj w/ a desc property and run method.
var gTests = [];
var gCurrentTest = null;

//------------------------------------------------------------------------------
// Bug 462662 - Pressing Enter to select tag from autocomplete closes bookmarks properties dialog
gTests.push({
  desc: "Bug 462662 - Pressing Enter to select tag from autocomplete closes bookmarks properties dialog",
  sidebar: SIDEBAR_BOOKMARKS_ID,
  action: ACTION_EDIT,
  itemType: null,
  window: null,
  _itemId: null,
  _cleanShutdown: false,

  setup: function(aCallback) {
    // Add a bookmark in unsorted bookmarks folder.
    this._itemId = add_bookmark(PlacesUtils._uri(TEST_URL));
    ok(this._itemId > 0, "Correctly added a bookmark");
    // Add a tag to this bookmark.
    PlacesUtils.tagging.tagURI(PlacesUtils._uri(TEST_URL),
                               ["testTag"]);
    var tags = PlacesUtils.tagging.getTagsForURI(PlacesUtils._uri(TEST_URL));
    is(tags[0], "testTag", "Correctly added a tag");
    aCallback();
  },

  selectNode: function(tree) {
    tree.selectItems([PlacesUtils.unfiledBookmarksFolderId]);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    tree.selectItems([this._itemId]);
    is(tree.selectedNode.itemId, this._itemId, "Bookmark has been selected");
  },

  run: function() {
    // open tags autocomplete and press enter
    var tagsField = this.window.document.getElementById("editBMPanel_tagsField");
    var self = this;

    this.window.addEventListener("unload", function(event) {
      self.window.removeEventListener("unload", arguments.callee, true);
      tagsField.popup.removeEventListener("popuphidden", popupListener, true);
      ok(self._cleanShutdown, "Dialog window should not be closed by pressing Enter on the autocomplete popup");
      executeSoon(function () {
        self.finish();
      });
    }, true);

    var popupListener = {
      handleEvent: function(aEvent) {
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
            var tree = tagsField.popup.tree;
            // Focus and select first result.
            isnot(tree, null, "Autocomplete results tree exists");
            is(tree.view.rowCount, 1, "We have 1 autocomplete result");
            tagsField.popup.selectedIndex = 0;
            is(tree.view.selection.count, 1,
               "We have selected a tag from the autocomplete popup");
            info("About to focus the autocomplete results tree");
            tree.focus();
            EventUtils.synthesizeKey("VK_RETURN", {}, self.window);
            break;
          default:
            ok(false, "unknown event: " + aEvent.type);
            return;
        }
      }
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
  },

  finish: function() {
    SidebarUI.hide();
    runNextTest();
  },

  cleanup: function() {
    // Check tags have not changed.
    var tags = PlacesUtils.tagging.getTagsForURI(PlacesUtils._uri(TEST_URL));
    is(tags[0], "testTag", "Tag on node has not changed");

    // Cleanup.
    PlacesUtils.tagging.untagURI(PlacesUtils._uri(TEST_URL), ["testTag"]);
    PlacesUtils.bookmarks.removeItem(this._itemId);
  }
});

//------------------------------------------------------------------------------
// Bug 476020 - Pressing Esc while having the tag autocomplete open closes the bookmarks panel

gTests.push({
  desc: "Bug 476020 - Pressing Esc while having the tag autocomplete open closes the bookmarks panel",
  sidebar: SIDEBAR_BOOKMARKS_ID,
  action: ACTION_EDIT,
  itemType: null,
  window: null,
  _itemId: null,
  _cleanShutdown: false,

  setup: function(aCallback) {
    // Add a bookmark in unsorted bookmarks folder.
    this._itemId = add_bookmark(PlacesUtils._uri(TEST_URL));
    ok(this._itemId > 0, "Correctly added a bookmark");
    // Add a tag to this bookmark.
    PlacesUtils.tagging.tagURI(PlacesUtils._uri(TEST_URL),
                               ["testTag"]);
    var tags = PlacesUtils.tagging.getTagsForURI(PlacesUtils._uri(TEST_URL));
    is(tags[0], "testTag", "Correctly added a tag");
    aCallback();
  },

  selectNode: function(tree) {
    tree.selectItems([PlacesUtils.unfiledBookmarksFolderId]);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    tree.selectItems([this._itemId]);
    is(tree.selectedNode.itemId, this._itemId, "Bookmark has been selected");
  },

  run: function() {
    // open tags autocomplete and press enter
    var tagsField = this.window.document.getElementById("editBMPanel_tagsField");
    var self = this;

    this.window.addEventListener("unload", function(event) {
      self.window.removeEventListener("unload", arguments.callee, true);
      tagsField.popup.removeEventListener("popuphidden", popupListener, true);
      ok(self._cleanShutdown, "Dialog window should not be closed by pressing Escape on the autocomplete popup");
      executeSoon(function () {
        self.finish();
      });
    }, true);

    var popupListener = {
      handleEvent: function(aEvent) {
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
            var tree = tagsField.popup.tree;
            // Focus and select first result.
            isnot(tree, null, "Autocomplete results tree exists");
            is(tree.view.rowCount, 1, "We have 1 autocomplete result");
            tagsField.popup.selectedIndex = 0;
            is(tree.view.selection.count, 1,
               "We have selected a tag from the autocomplete popup");
            info("About to focus the autocomplete results tree");
            tree.focus();
            EventUtils.synthesizeKey("VK_ESCAPE", {}, self.window);
            break;
          default:
            ok(false, "unknown event: " + aEvent.type);
            return;
        }
      }
    };
    tagsField.popup.addEventListener("popupshown", popupListener, true);
    tagsField.popup.addEventListener("popuphidden", popupListener, true);

    // Open tags autocomplete popup.
    info("About to focus the tagsField");
    tagsField.focus();
    tagsField.value = "";
    EventUtils.synthesizeKey("t", {}, this.window);
  },

  finish: function() {
    SidebarUI.hide();
    runNextTest();
  },

  cleanup: function() {
    // Check tags have not changed.
    var tags = PlacesUtils.tagging.getTagsForURI(PlacesUtils._uri(TEST_URL));
    is(tags[0], "testTag", "Tag on node has not changed");

    // Cleanup.
    PlacesUtils.tagging.untagURI(PlacesUtils._uri(TEST_URL),
                                 ["testTag"]);
    PlacesUtils.bookmarks.removeItem(this._itemId);
  }
});

//------------------------------------------------------------------------------
//  Bug 491269 - Test that editing folder name in bookmarks properties dialog does not accept the dialog

gTests.push({
  desc: " Bug 491269 - Test that editing folder name in bookmarks properties dialog does not accept the dialog",
  sidebar: SIDEBAR_HISTORY_ID,
  action: ACTION_ADD,
  historyView: SIDEBAR_HISTORY_BYLASTVISITED_VIEW,
  window: null,

  setup: function(aCallback) {
    // Add a visit.
    PlacesTestUtils.addVisits(
      {uri: PlacesUtils._uri(TEST_URL),
        transition: PlacesUtils.history.TRANSITION_TYPED}
      ).then(aCallback);
  },

  selectNode: function(tree) {
    var visitNode = tree.view.nodeForTreeIndex(0);
    tree.selectNode(visitNode);
    is(tree.selectedNode.uri, TEST_URL, "The correct visit has been selected");
    is(tree.selectedNode.itemId, -1, "The selected node is not bookmarked");
  },

  run: function() {
    // Open folder selector.
    var foldersExpander = this.window.document.getElementById("editBMPanel_foldersExpander");
    var folderTree = this.window.document.getElementById("editBMPanel_folderTree");
    var self = this;

    this.window.addEventListener("unload", function(event) {
      self.window.removeEventListener("unload", arguments.callee, true);
      ok(self._cleanShutdown, "Dialog window should not be closed by pressing ESC in folder name textbox");
      executeSoon(function () {
        self.finish();
      });
    }, true);

    folderTree.addEventListener("DOMAttrModified", function onDOMAttrModified(event) {
      if (event.attrName != "place")
        return;
      folderTree.removeEventListener("DOMAttrModified", arguments.callee, false);
      executeSoon(function () {
        // Create a new folder.
        var newFolderButton = self.window.document.getElementById("editBMPanel_newFolderButton");
        newFolderButton.doCommand();
        ok(folderTree.hasAttribute("editing"),
           "We are editing new folder name in folder tree");

        // Press Escape to discard editing new folder name.
        EventUtils.synthesizeKey("VK_ESCAPE", {}, self.window);
        ok(!folderTree.hasAttribute("editing"),
           "We have finished editing folder name in folder tree");
        self._cleanShutdown = true;
        self.window.document.documentElement.cancelDialog();
      });
    }, false);
    foldersExpander.doCommand();
  },

  finish: function() {
    SidebarUI.hide();
    runNextTest();
  },

  cleanup: function() {
    return PlacesTestUtils.clearHistory();
  }
});

//------------------------------------------------------------------------------

function test() {
  waitForExplicitFinish();
  // This test can take some time, if we timeout too early it could run
  // in the middle of other tests, or hang them.
  requestLongerTimeout(2);

  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils in context");
  ok(PlacesUIUtils, "PlacesUIUtils in context");

  // kick off tests
  runNextTest();
}

function runNextTest() {
  // Cleanup from previous test.
  if (gCurrentTest) {
    Promise.resolve(gCurrentTest.cleanup()).then(() => {
      info("End of test: " + gCurrentTest.desc);
      gCurrentTest = null;
      waitForAsyncUpdates(runNextTest);
    });
    return;
  }

  if (gTests.length > 0) {
    // Goto next tests.
    gCurrentTest = gTests.shift();
    info("Start of test: " + gCurrentTest.desc);
    gCurrentTest.setup(function() {
      execute_test_in_sidebar();
    });
  }
  else {
    // Finished all tests.
    finish();
  }
}

/**
 * Global functions to run a test in Properties dialog context.
 */

function execute_test_in_sidebar() {
    var sidebar = document.getElementById("sidebar");
    sidebar.addEventListener("load", function() {
      sidebar.removeEventListener("load", arguments.callee, true);
      // Need to executeSoon since the tree is initialized on sidebar load.
      executeSoon(open_properties_dialog);
    }, true);
    SidebarUI.show(gCurrentTest.sidebar);
}

function open_properties_dialog() {
    var sidebar = document.getElementById("sidebar");

    // If this is history sidebar, set the required view.
    if (gCurrentTest.sidebar == SIDEBAR_HISTORY_ID)
      sidebar.contentDocument.getElementById(gCurrentTest.historyView).doCommand();

    // Get sidebar's Places tree.
    var sidebarTreeID = gCurrentTest.sidebar == SIDEBAR_BOOKMARKS_ID ?
                                                SIDEBAR_BOOKMARKS_TREE_ID :
                                                SIDEBAR_HISTORY_TREE_ID;
    var tree = sidebar.contentDocument.getElementById(sidebarTreeID);
    ok(tree, "Sidebar tree has been loaded");

    // Ask current test to select the node to edit.
    gCurrentTest.selectNode(tree);
    ok(tree.selectedNode,
       "We have a places node selected: " + tree.selectedNode.title);

    // Wait for the Properties dialog.
    function windowObserver(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened")
        return;
      ww.unregisterNotification(windowObserver);
      let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
      waitForFocus(() => {
        // Windows has been loaded, execute our test now.
        executeSoon(function () {
          // Ensure overlay is loaded
          ok(win.gEditItemOverlay.initialized, "EditItemOverlay is initialized");
          gCurrentTest.window = win;
          try {
            gCurrentTest.run();
          } catch (ex) {
            ok(false, "An error occured during test run: " + ex.message);
          }
        });
      }, win);
    }
    ww.registerNotification(windowObserver);

    var command = null;
    switch (gCurrentTest.action) {
      case ACTION_EDIT:
        command = "placesCmd_show:info";
        break;
      case ACTION_ADD:
        if (gCurrentTest.sidebar == SIDEBAR_BOOKMARKS_ID) {
          if (gCurrentTest.itemType == TYPE_FOLDER)
            command = "placesCmd_new:folder";
          else if (gCurrentTest.itemType == TYPE_BOOKMARK)
            command = "placesCmd_new:bookmark";
          else
            ok(false, "You didn't set a valid itemType for adding an item");
        }
        else
          command = "placesCmd_createBookmark";
        break;
      default:
        ok(false, "You didn't set a valid action for this test");
    }
    // Ensure command is enabled for this node.
    ok(tree.controller.isCommandEnabled(command),
       " command '" + command + "' on current selected node is enabled");

    // This will open the dialog.
    tree.controller.doCommand(command);
}
