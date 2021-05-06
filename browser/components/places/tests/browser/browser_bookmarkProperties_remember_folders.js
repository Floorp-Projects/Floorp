/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

/**
 * Tests that multiple tags can be added to a bookmark using the star-shaped button, the library and the sidebar.
 */

StarUI._createPanelIfNeeded();
const bookmarkPanel = document.getElementById("editBookmarkPanel");
let folders;

async function openPopupAndSelectFolder(guid, newBookmark = false) {
  await clickBookmarkStar();

  let notificationPromise;
  if (!newBookmark) {
    notificationPromise = PlacesTestUtils.waitForNotification(
      "bookmark-moved",
      events => events.some(e => guid === e.parentGuid),
      "places"
    );
  }

  // Expand the folder tree.
  document.getElementById("editBMPanel_foldersExpander").click();
  document.getElementById("editBMPanel_folderTree").selectItems([guid]);

  await hideBookmarksPanel();
  if (!newBookmark) {
    await notificationPromise;
  }
}

async function assertRecentFolders(expectedGuids, msg) {
  await clickBookmarkStar();

  let actualGuids = [];
  function getGuids() {
    actualGuids = [];
    const folderMenuPopup = document.getElementById(
      "editBMPanel_folderMenuList"
    ).menupopup;

    let separatorFound = false;
    // The list of folders goes from editBMPanel_foldersSeparator to the end.
    for (let child of folderMenuPopup.children) {
      if (separatorFound) {
        actualGuids.push(child.folderGuid);
      } else if (child.id == "editBMPanel_foldersSeparator") {
        separatorFound = true;
      }
    }
  }

  // The dialog fills in the folder list asnychronously, so we might need to wait
  // for that to complete.
  await TestUtils.waitForCondition(() => {
    getGuids();
    return actualGuids.length == expectedGuids.length;
  }, `Should have opened dialog with expected recent folders for: ${msg}`);

  Assert.deepEqual(actualGuids, expectedGuids, msg);

  await hideBookmarksPanel();

  // Give the metadata chance to be written to the database before we attempt
  // to open the dialog again.
  let diskGuids = [];
  await TestUtils.waitForCondition(async () => {
    diskGuids = await PlacesUtils.metadata.get(
      PlacesUIUtils.LAST_USED_FOLDERS_META_KEY,
      []
    );
    return diskGuids.length == expectedGuids.length;
  }, `Should have written data to disk for: ${msg}`);

  Assert.deepEqual(
    diskGuids,
    expectedGuids,
    `Should match the disk GUIDS for ${msg}`
  );
}

add_task(async function setup() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.metadata.delete(PlacesUIUtils.LAST_USED_FOLDERS_META_KEY);

  bookmarkPanel.setAttribute("animate", false);

  let oldTimeout = StarUI._autoCloseTimeout;
  // Make the timeout something big, so it doesn't iteract badly with tests.
  StarUI._autoCloseTimeout = 6000000;

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:robots",
    waitForStateStop: true,
  });

  folders = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "Bob",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
      {
        title: "Place",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
      {
        title: "Delight",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
      {
        title: "Surprise",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
      {
        title: "Treble Bob",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
      {
        title: "Principal",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
      {
        title: "Max Default Recent Folders",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
      {
        title: "One Over Default Maximum Recent Folders",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
    ],
  });

  registerCleanupFunction(async () => {
    StarUI._autoCloseTimeout = oldTimeout;
    BrowserTestUtils.removeTab(tab);
    bookmarkPanel.removeAttribute("animate");
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.metadata.delete(PlacesUIUtils.LAST_USED_FOLDERS_META_KEY);
  });
});

add_task(async function test_remember_last_folder() {
  await assertRecentFolders([], "Should have no recent folders to start with.");

  await openPopupAndSelectFolder(folders[0].guid, true);

  await assertRecentFolders(
    [folders[0].guid],
    "Should have one folder in the list."
  );
});

add_task(async function test_forget_oldest_folder() {
  // Add some more folders.
  let expectedFolders = [folders[0].guid];
  for (let i = 1; i < folders.length; i++) {
    await assertRecentFolders(
      expectedFolders,
      "Should have only the expected folders in the list"
    );

    await openPopupAndSelectFolder(folders[i].guid);

    expectedFolders.unshift(folders[i].guid);
    if (expectedFolders.length > PlacesUIUtils.maxRecentFolders) {
      expectedFolders.pop();
    }
  }

  await assertRecentFolders(
    expectedFolders,
    "Should have expired the original folder"
  );
});

add_task(async function test_reorder_folders() {
  let expectedFolders = [
    folders[2].guid,
    folders[7].guid,
    folders[6].guid,
    folders[5].guid,
    folders[4].guid,
    folders[3].guid,
    folders[1].guid,
  ];

  // Take an old one and put it at the front.
  await openPopupAndSelectFolder(folders[2].guid);

  await assertRecentFolders(
    expectedFolders,
    "Should have correctly re-ordered the list"
  );
});

add_task(async function test_change_max_recent_folders_pref() {
  let expectedFolders = [folders[0].guid];

  Services.prefs.setIntPref("browser.bookmarks.editDialog.maxRecentFolders", 1);

  await openPopupAndSelectFolder(folders[1].guid);
  await openPopupAndSelectFolder(folders[0].guid);

  await assertRecentFolders(
    expectedFolders,
    "Should have only one recent folder in the bookmark edit panel"
  );

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref(
      "browser.bookmarks.editDialog.maxRecentFolders"
    );
  });
});
