/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests bookmarks backup export/import as JSON file.
 */

const BASE_URL = "http://example.com/";

const PLACES = [
  {
    guid: PlacesUtils.bookmarks.menuGuid,
    prefix: "In Menu",
    total: 5,
  },
  {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    prefix: "In Toolbar",
    total: 7,
  },
  {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    prefix: "In Other",
    total: 8,
  },
];

var importExportPicker, saveDir, actualBookmarks;

async function generateTestBookmarks() {
  actualBookmarks = [];
  for (let place of PLACES) {
    let currentPlaceChildren = [];
    for (let i = 1; i <= place.total; i++) {
      currentPlaceChildren.push({
        url: `${BASE_URL}${i}`,
        title: `${place.prefix} Bookmark: ${i}`,
      });
    }
    await PlacesUtils.bookmarks.insertTree({
      guid: place.guid,
      children: currentPlaceChildren,
    });
    actualBookmarks = actualBookmarks.concat(currentPlaceChildren);
  }
}

async function validateImportedBookmarksByParent(
  parentGuid,
  expectedChildrenTotal
) {
  let currentPlace = PLACES.filter(elem => {
    return elem.guid === parentGuid.toString();
  })[0];

  let bookmarksTree = await PlacesUtils.promiseBookmarksTree(parentGuid);

  Assert.equal(
    bookmarksTree.children.length,
    expectedChildrenTotal,
    `Imported bookmarks length should be ${expectedChildrenTotal}`
  );

  for (let importedBookmark of bookmarksTree.children) {
    Assert.equal(
      importedBookmark.type,
      PlacesUtils.TYPE_X_MOZ_PLACE,
      `Exported bookmarks should be of type bookmark`
    );

    let doesTitleContain = importedBookmark.title
      .toString()
      .includes(`${currentPlace.prefix} Bookmark`);
    Assert.equal(
      doesTitleContain,
      true,
      `Bookmark title should contain text: ${currentPlace.prefix} Bookmark`
    );

    let doesUriContains = importedBookmark.uri.toString().includes(BASE_URL);
    Assert.equal(doesUriContains, true, "Bookmark uri should contain base url");
  }
}

async function validateImportedBookmarks(fromPlaces) {
  for (let i = 0; i < fromPlaces.length; i++) {
    let parentContainer = fromPlaces[i];
    await validateImportedBookmarksByParent(
      parentContainer.guid,
      parentContainer.total
    );
  }
}

async function promiseImportExport() {
  saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("temp-bookmarks-export");
  if (!saveDir.exists()) {
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  importExportPicker.displayDirectory = saveDir;

  return new Promise(resolve => {
    importExportPicker.showCallback = async () => {
      let fileName = "bookmarks-backup.json";
      let destFile = saveDir.clone();
      destFile.append(fileName);
      importExportPicker.setFiles([destFile]);
      resolve(destFile);
    };
  });
}

add_setup(async function () {
  await promisePlacesInitComplete();
  await PlacesUtils.bookmarks.eraseEverything();
  await generateTestBookmarks();
  importExportPicker = SpecialPowers.MockFilePicker;
  importExportPicker.init(window);

  registerCleanupFunction(async () => {
    importExportPicker.cleanup();
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

async function showMaintenancePopup(libraryWindow) {
  let button = libraryWindow.document.getElementById("maintenanceButton");
  let popup = libraryWindow.document.getElementById("maintenanceButtonPopup");
  let shown = BrowserTestUtils.waitForEvent(popup, "popupshown");

  info("Clicking maintenance menu");

  button.openMenu(true);

  await shown;
  info("Maintenance popup shown");
  return popup;
}

add_task(async function test_export_json() {
  let libraryWindow = await promiseLibrary();
  let popup = await showMaintenancePopup(libraryWindow);
  let hidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");

  info("Activating #backupBookmarks");

  let backupPromise = promiseImportExport();

  popup.activateItem(popup.querySelector("#backupBookmarks"));
  await hidden;

  info("Popup hidden");

  let backupFile = await backupPromise;
  await TestUtils.waitForCondition(
    backupFile.exists,
    "Backup file should exist"
  );
  await promiseLibraryClosed(libraryWindow);

  await PlacesUtils.bookmarks.eraseEverything();
});

async function showFileRestorePopup(libraryWindow) {
  let parentPopup = await showMaintenancePopup(libraryWindow);
  let popup = parentPopup.querySelector("#fileRestorePopup");
  let shown = BrowserTestUtils.waitForEvent(popup, "popupshown");
  parentPopup.querySelector("#fileRestoreMenu").openMenu(true);
  await shown;
  return popup;
}

add_task(async function test_import_json() {
  let libraryWindow = await promiseLibrary();
  let popup = await showFileRestorePopup(libraryWindow);

  let backupPromise = promiseImportExport();
  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen("accept");

  let hidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  popup.activateItem(popup.querySelector("#restoreFromFile"));
  await hidden;

  await backupPromise;
  await dialogPromise;

  let restored = 0;
  let promiseBookmarksRestored = PlacesTestUtils.waitForNotification(
    "bookmark-added",
    events => events.some(() => ++restored == actualBookmarks.length)
  );

  await promiseBookmarksRestored;
  await validateImportedBookmarks(PLACES);
  await promiseLibraryClosed(libraryWindow);

  registerCleanupFunction(async () => {
    if (saveDir) {
      saveDir.remove(true);
    }
  });
});
