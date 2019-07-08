/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 651942 */

// Reference to the Scratchpad object.
var gScratchpad;

// References to the temporary nsIFiles.
var gFile01;
var gFile02;
var gFile03;
var gFile04;

// lists of recent files.
var lists = {
  recentFiles01: null,
  recentFiles02: null,
  recentFiles03: null,
  recentFiles04: null,
};

// Temporary file names.
var gFileName01 = "file01_ForBug651942.tmp";
var gFileName02 = "☕"; // See bug 783858 for more information
var gFileName03 = "file03_ForBug651942.tmp";
var gFileName04 = "file04_ForBug651942.tmp";

// Content for the temporary files.
var gFileContent01 = "hello.world.01('bug651942');";
var gFileContent02 = "hello.world.02('bug651942');";
var gFileContent03 = "hello.world.03('bug651942');";
var gFileContent04 = "hello.world.04('bug651942');";

// Return a promise that will be resolved after one event loop turn.
function snooze() {
  return new Promise(resolve => setTimeout(resolve, 0));
}

async function testAddedToRecent() {
  gScratchpad = gScratchpadWindow.Scratchpad;

  gFile01 = await createAndLoadTemporaryFile(gFileName01, gFileContent01);
  gFile02 = await createAndLoadTemporaryFile(gFileName02, gFileContent02);
  gFile03 = await createAndLoadTemporaryFile(gFileName03, gFileContent03);

  // Test to see if the files we just created have been added to the list of
  // recent files.
  lists.recentFiles01 = gScratchpad.getRecentFiles();

  is(
    lists.recentFiles01.length,
    3,
    "Temporary files created successfully and added to list of recent files."
  );

  // Create a 4th file, this should clear the oldest file.
  gFile04 = await createAndLoadTemporaryFile(gFileName04, gFileContent04);

  // Test to see if the oldest recent file was removed,
  // and that the other files were reordered successfully.
  lists.recentFiles02 = gScratchpad.getRecentFiles();

  is(
    lists.recentFiles02[0],
    lists.recentFiles01[1],
    "File02 was reordered successfully in the 'recent files'-list."
  );
  is(
    lists.recentFiles02[1],
    lists.recentFiles01[2],
    "File03 was reordered successfully in the 'recent files'-list."
  );
  isnot(
    lists.recentFiles02[2],
    lists.recentFiles01[2],
    "File04: was added successfully."
  );

  // Open the oldest recent file.
  await gScratchpad.openFile(0);

  // Test to see if it is now the most recent file, and that the other files
  // were reordered successfully.
  lists.recentFiles03 = gScratchpad.getRecentFiles();

  is(
    lists.recentFiles02[0],
    lists.recentFiles03[2],
    "File04 was reordered successfully in the 'recent files'-list."
  );
  is(
    lists.recentFiles02[1],
    lists.recentFiles03[0],
    "File03 was reordered successfully in the 'recent files'-list."
  );
  is(
    lists.recentFiles02[2],
    lists.recentFiles03[1],
    "File02 was reordered successfully in the 'recent files'-list."
  );
}

async function testHideMenu() {
  Services.prefs.setIntPref("devtools.scratchpad.recentFilesMax", 0);

  // Give the Scratchpad UI time to react to the pref change, via its observer.
  // This is a race condition; to fix it, Scratchpad would need to give us some
  // indication that it was finished responding to a pref update - perhaps by
  // emitting an event.
  await snooze();

  // The "devtools.scratchpad.recentFilesMax"-preference was set to zero (0).
  // This should disable the "Open Recent"-menu by hiding it (this should not
  // remove any files from the list). Test to see if it's been hidden.
  const menu = gScratchpadWindow.document.getElementById("sp-open_recent-menu");
  ok(menu.hasAttribute("hidden"), "The menu was hidden successfully.");
}

async function testEnableMenu() {
  Services.prefs.setIntPref("devtools.scratchpad.recentFilesMax", 2);

  // Give the Scratchpad UI time to react to the pref change. This is a race
  // condition; see the comment in testHideMenu.
  await snooze();

  // We have set the recentFilesMax pref to a nonzero value. This enables the
  // feature, removes the oldest files, rebuilds the menu and removes the
  // "hidden"-attribute from it. Test to see if this works.
  const menu = gScratchpadWindow.document.getElementById("sp-open_recent-menu");
  ok(!menu.hasAttribute("hidden"), "The menu is visible. \\o/");

  lists.recentFiles04 = gScratchpad.getRecentFiles();

  is(
    lists.recentFiles04.length,
    2,
    "Two recent files were successfully removed from the 'recent files'-list"
  );

  const doc = gScratchpadWindow.document;
  const popup = doc.getElementById("sp-menu-open_recentPopup");

  const menuitemLabel = popup.children[0].getAttribute("label");
  let correctMenuItem = false;
  if (
    menuitemLabel === lists.recentFiles03[2] &&
    menuitemLabel === lists.recentFiles04[1]
  ) {
    correctMenuItem = true;
  }

  is(
    correctMenuItem,
    true,
    "Two recent files were successfully removed from the 'Open Recent'-menu"
  );
}

async function testOpenDeletedFile() {
  // We now remove one file from the harddrive and use the recent-menuitem for
  // it to make sure the user is notified that the file no longer exists.
  // This is tested in testOpenDeletedFile().
  gFile04.remove(false);

  // Make sure the file has been deleted before continuing.
  while (gFile04.exists()) {
    await snooze();
  }
  gFile04 = null;

  // By now we should have two recent files stored in the list but one of the
  // files should be missing on the harddrive.
  await gScratchpad.openFile(0);

  const doc = gScratchpadWindow.document;
  const popup = doc.getElementById("sp-menu-open_recentPopup");

  is(
    gScratchpad.getRecentFiles().length,
    1,
    "The missing file was successfully removed from the list."
  );
  // The number of recent files stored, plus the separator and the
  // clearRecentMenuItems-item.
  is(
    popup.children.length,
    3,
    "The missing file was successfully removed from the menu."
  );
  ok(
    gScratchpad.notificationBox.currentNotification,
    "The notification was successfully displayed."
  );
  is(
    gScratchpad.notificationBox.currentNotification.messageText.textContent,
    gScratchpad.strings.GetStringFromName("fileNoLongerExists.notification"),
    "The notification label is correct."
  );
}

async function testClearAll() {
  gScratchpad.clearRecentFiles();

  // Give the UI time to react to the recent files being cleared out. This is a
  // race condition. The clearRecentFiles method works asynchronously, but
  // there is no way to wait for it to finish. A single event loop turn should
  // be good enough.
  await snooze();

  // We have cleared the last file. Test to see if the last file was removed,
  // the menu is empty and was disabled successfully.
  const doc = gScratchpadWindow.document;
  const menu = doc.getElementById("sp-open_recent-menu");
  const popup = doc.getElementById("sp-menu-open_recentPopup");

  is(
    gScratchpad.getRecentFiles().length,
    0,
    "All recent files removed successfully."
  );
  is(popup.children.length, 0, "All menuitems removed successfully.");
  ok(
    menu.hasAttribute("disabled"),
    "No files in the menu, it was disabled successfully."
  );
}

async function createAndLoadTemporaryFile(aFileName, aFileContent) {
  info(`Create file: ${aFileName}`);

  // Create a temporary file.
  const aFile = FileUtils.getFile("TmpD", [aFileName]);
  aFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  // Write the temporary file.
  await OS.File.writeAtomic(aFile.path, aFileContent);

  gScratchpad.setFilename(aFile.path);
  let [status] = await gScratchpad.importFromFile(
    aFile.QueryInterface(Ci.nsIFile),
    true
  );
  ok(
    Components.isSuccessCode(status),
    "the temporary file was imported successfully with Scratchpad"
  );
  status = await gScratchpad.saveFile();
  ok(
    Components.isSuccessCode(status),
    "the temporary file was saved successfully with Scratchpad"
  );
  checkIfMenuIsPopulated();

  info(`File created: ${aFileName}`);
  return aFile;
}

function checkIfMenuIsPopulated() {
  const doc = gScratchpadWindow.document;
  const expectedMenuitemCount = doc.getElementById("sp-menu-open_recentPopup")
    .children.length;
  // The number of recent files stored, plus the separator and the
  // clearRecentMenuItems-item.
  const recentFilesPlusExtra = gScratchpad.getRecentFiles().length + 2;

  if (expectedMenuitemCount > 2) {
    is(
      expectedMenuitemCount,
      recentFilesPlusExtra,
      "the recent files menu was populated successfully."
    );
  }
}

async function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    gFile01.remove(false);
    gFile02.remove(false);
    gFile03.remove(false);
    // If all tests passed, gFile04 was already removed, but just in case:
    if (gFile04) {
      gFile04.remove(false);
    }
    Services.prefs.clearUserPref("devtools.scratchpad.recentFilesMax");
  });

  Services.prefs.setIntPref("devtools.scratchpad.recentFilesMax", 3);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.loadURI(
    gBrowser,
    "data:text/html,<p>test recent files in Scratchpad"
  );
  await loaded;
  await new Promise(openScratchpad);

  await testAddedToRecent();
  await testHideMenu();
  await testEnableMenu();
  await testOpenDeletedFile();
  await testClearAll();

  finish();
}
