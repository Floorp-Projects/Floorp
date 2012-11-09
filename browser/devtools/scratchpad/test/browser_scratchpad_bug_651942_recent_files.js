/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {};
Cu.import("resource://gre/modules/NetUtil.jsm", tempScope);
Cu.import("resource://gre/modules/FileUtils.jsm", tempScope);
let NetUtil = tempScope.NetUtil;
let FileUtils = tempScope.FileUtils;

// Reference to the Scratchpad object.
let gScratchpad;

// References to the temporary nsIFiles.
let gFile01;
let gFile02;
let gFile03;
let gFile04;

// lists of recent files.
var lists = {
  recentFiles01: null,
  recentFiles02: null,
  recentFiles03: null,
  recentFiles04: null,
};

// Temporary file names.
let gFileName01 = "file01_ForBug651942.tmp"
let gFileName02 = "☕" // See bug 783858 for more information
let gFileName03 = "file03_ForBug651942.tmp"
let gFileName04 = "file04_ForBug651942.tmp"

// Content for the temporary files.
let gFileContent;
let gFileContent01 = "hello.world.01('bug651942');";
let gFileContent02 = "hello.world.02('bug651942');";
let gFileContent03 = "hello.world.03('bug651942');";
let gFileContent04 = "hello.world.04('bug651942');";

function startTest()
{
  gScratchpad = gScratchpadWindow.Scratchpad;

  gFile01 = createAndLoadTemporaryFile(gFile01, gFileName01, gFileContent01);
  gFile02 = createAndLoadTemporaryFile(gFile02, gFileName02, gFileContent02);
  gFile03 = createAndLoadTemporaryFile(gFile03, gFileName03, gFileContent03);
}

// Test to see if the three files we created in the 'startTest()'-method have
// been added to the list of recent files.
function testAddedToRecent()
{
  lists.recentFiles01 = gScratchpad.getRecentFiles();

  is(lists.recentFiles01.length, 3,
     "Temporary files created successfully and added to list of recent files.");

  // Create a 4th file, this should clear the oldest file.
  gFile04 = createAndLoadTemporaryFile(gFile04, gFileName04, gFileContent04);
}

// We have opened a 4th file. Test to see if the oldest recent file was removed,
// and that the other files were reordered successfully.
function testOverwriteRecent()
{
  lists.recentFiles02 = gScratchpad.getRecentFiles();

  is(lists.recentFiles02[0], lists.recentFiles01[1],
     "File02 was reordered successfully in the 'recent files'-list.");
  is(lists.recentFiles02[1], lists.recentFiles01[2],
     "File03 was reordered successfully in the 'recent files'-list.");
  isnot(lists.recentFiles02[2], lists.recentFiles01[2],
        "File04: was added successfully.");

  // Open the oldest recent file.
  gScratchpad.openFile(0);
}

// We have opened the "oldest"-recent file. Test to see if it is now the most
// recent file, and that the other files were reordered successfully.
function testOpenOldestRecent()
{
  lists.recentFiles03 = gScratchpad.getRecentFiles();

  is(lists.recentFiles02[0], lists.recentFiles03[2],
     "File04 was reordered successfully in the 'recent files'-list.");
  is(lists.recentFiles02[1], lists.recentFiles03[0],
     "File03 was reordered successfully in the 'recent files'-list.");
  is(lists.recentFiles02[2], lists.recentFiles03[1],
     "File02 was reordered successfully in the 'recent files'-list.");

  Services.prefs.setIntPref("devtools.scratchpad.recentFilesMax", 0);
}

// The "devtools.scratchpad.recentFilesMax"-preference was set to zero (0).
// This should disable the "Open Recent"-menu by hiding it (this should not
// remove any files from the list). Test to see if it's been hidden.
function testHideMenu()
{
  let menu = gScratchpadWindow.document.getElementById("sp-open_recent-menu");
  ok(menu.hasAttribute("hidden"), "The menu was hidden successfully.");

  Services.prefs.setIntPref("devtools.scratchpad.recentFilesMax", 2);
}

// We have set the recentFilesMax-pref to one (1), this enables the feature,
// removes the two oldest files, rebuilds the menu and removes the
// "hidden"-attribute from it. Test to see if this works.
function testChangedMaxRecent()
{
  let menu = gScratchpadWindow.document.getElementById("sp-open_recent-menu");
  ok(!menu.hasAttribute("hidden"), "The menu is visible. \\o/");

  lists.recentFiles04 = gScratchpad.getRecentFiles();

  is(lists.recentFiles04.length, 2,
     "Two recent files were successfully removed from the 'recent files'-list");

  let doc = gScratchpadWindow.document;
  let popup = doc.getElementById("sp-menu-open_recentPopup");

  let menuitemLabel = popup.children[0].getAttribute("label");
  let correctMenuItem = false;
  if (menuitemLabel === lists.recentFiles03[2] &&
      menuitemLabel === lists.recentFiles04[1]) {
    correctMenuItem = true;
  }

  is(correctMenuItem, true,
     "Two recent files were successfully removed from the 'Open Recent'-menu");

  // We now remove one file from the harddrive and use the recent-menuitem for
  // it to make sure the user is notified that the file no longer exists.
  // This is tested in testOpenDeletedFile().
  gFile04.remove(false);

  // Make sure the file has been deleted before continuing to avoid
  // intermittent oranges.
  waitForFileDeletion();
}

function waitForFileDeletion() {
  if (gFile04.exists()) {
    executeSoon(waitForFileDeletion);
    return;
  }

  gFile04 = null;
  gScratchpad.openFile(0);
}

// By now we should have two recent files stored in the list but one of the
// files should be missing on the harddrive.
function testOpenDeletedFile() {
  let doc = gScratchpadWindow.document;
  let popup = doc.getElementById("sp-menu-open_recentPopup");

  is(gScratchpad.getRecentFiles().length, 1,
     "The missing file was successfully removed from the list.");
  // The number of recent files stored, plus the separator and the
  // clearRecentMenuItems-item.
  is(popup.children.length, 3,
     "The missing file was successfully removed from the menu.");
  ok(gScratchpad.notificationBox.currentNotification,
     "The notification was successfully displayed.");
  is(gScratchpad.notificationBox.currentNotification.label,
     gScratchpad.strings.GetStringFromName("fileNoLongerExists.notification"),
     "The notification label is correct.");

  gScratchpad.clearRecentFiles();
}

// We have cleared the last file. Test to see if the last file was removed,
// the menu is empty and was disabled successfully.
function testClearedAll()
{
  let doc = gScratchpadWindow.document;
  let menu = doc.getElementById("sp-open_recent-menu");
  let popup = doc.getElementById("sp-menu-open_recentPopup");

  is(gScratchpad.getRecentFiles().length, 0,
     "All recent files removed successfully.");
  is(popup.children.length, 0, "All menuitems removed successfully.");
  ok(menu.hasAttribute("disabled"),
     "No files in the menu, it was disabled successfully.");

  finishTest();
}

function createAndLoadTemporaryFile(aFile, aFileName, aFileContent)
{
  // Create a temporary file.
  aFile = FileUtils.getFile("TmpD", [aFileName]);
  aFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);

  // Write the temporary file.
  let fout = Cc["@mozilla.org/network/file-output-stream;1"].
             createInstance(Ci.nsIFileOutputStream);
  fout.init(aFile.QueryInterface(Ci.nsILocalFile), 0x02 | 0x08 | 0x20,
            0644, fout.DEFER_OPEN);

  gScratchpad.setFilename(aFile.path);
  gScratchpad.importFromFile(aFile.QueryInterface(Ci.nsILocalFile),  true,
                            fileImported);
  gScratchpad.saveFile(fileSaved);

  return aFile;
}

function fileImported(aStatus)
{
  ok(Components.isSuccessCode(aStatus),
     "the temporary file was imported successfully with Scratchpad");
}

function fileSaved(aStatus)
{
  ok(Components.isSuccessCode(aStatus),
     "the temporary file was saved successfully with Scratchpad");

  checkIfMenuIsPopulated();
}

function checkIfMenuIsPopulated()
{
  let doc = gScratchpadWindow.document;
  let expectedMenuitemCount = doc.getElementById("sp-menu-open_recentPopup").
                              children.length;
  // The number of recent files stored, plus the separator and the
  // clearRecentMenuItems-item.
  let recentFilesPlusExtra = gScratchpad.getRecentFiles().length + 2;

  if (expectedMenuitemCount > 2) {
    is(expectedMenuitemCount, recentFilesPlusExtra,
       "the recent files menu was populated successfully.");
  }
}

/**
 * The PreferenceObserver listens for preference changes while Scratchpad is
 * running.
 */
var PreferenceObserver = {
  _initialized: false,

  _timesFired: 0,
  set timesFired(aNewValue) {
    this._timesFired = aNewValue;
  },
  get timesFired() {
    return this._timesFired;
  },

  init: function PO_init()
  {
    if (this._initialized) {
      return;
    }

    this.branch = Services.prefs.getBranch("devtools.scratchpad.");
    this.branch.addObserver("", this, false);
    this._initialized = true;
  },

  observe: function PO_observe(aMessage, aTopic, aData)
  {
    if (aTopic != "nsPref:changed") {
      return;
    }

    switch (this.timesFired) {
      case 0:
        this.timesFired = 1;
        break;
      case 1:
        this.timesFired = 2;
        break;
      case 2:
        this.timesFired = 3;
        testAddedToRecent();
        break;
      case 3:
        this.timesFired = 4;
        testOverwriteRecent();
        break;
      case 4:
        this.timesFired = 5;
        testOpenOldestRecent();
        break;
      case 5:
        this.timesFired = 6;
        testHideMenu();
        break;
      case 6:
        this.timesFired = 7;
        testChangedMaxRecent();
        break;
      case 7:
        this.timesFired = 8;
        testOpenDeletedFile();
        break;
      case 8:
        this.timesFired = 9;
        testClearedAll();
        break;
    }
  },

  uninit: function PO_uninit () {
    this.branch.removeObserver("", this);
  }
};

function test()
{
  waitForExplicitFinish();

  registerCleanupFunction(function () {
    gFile01.remove(false);
    gFile01 = null;
    gFile02.remove(false);
    gFile02 = null;
    gFile03.remove(false);
    gFile03 = null;
    // gFile04 was removed earlier.
    lists.recentFiles01 = null;
    lists.recentFiles02 = null;
    lists.recentFiles03 = null;
    lists.recentFiles04 = null;
    gScratchpad = null;

    PreferenceObserver.uninit();
    Services.prefs.clearUserPref("devtools.scratchpad.recentFilesMax");
  });

  Services.prefs.setIntPref("devtools.scratchpad.recentFilesMax", 3);

  // Initiate the preference observer after we have set the temporary recent
  // files max for this test.
  PreferenceObserver.init();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(startTest);
  }, true);

  content.location = "data:text/html,<p>test recent files in Scratchpad";
}

function finishTest()
{
  finish();
}
