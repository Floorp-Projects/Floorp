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
    total: 5
  },
  {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    prefix: "In Toolbar",
    total: 7
  },
  {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    prefix: "In Other",
    total: 8
  }
];

var importExportPicker, saveDir, actualBookmarks;

async function generateTestBookmarks() {
  actualBookmarks = [];
  for (let place of PLACES) {
    let currentPlaceChildren = [];
    for (let i = 1; i <= place.total; i++) {
      currentPlaceChildren.push({
        url: `${BASE_URL}${i}`,
        title: `${place.prefix} Bookmark: ${i}`
      });
    }
    await PlacesUtils.bookmarks.insertTree({
      guid: place.guid,
      children: currentPlaceChildren
    });
    actualBookmarks = actualBookmarks.concat(currentPlaceChildren);
  }
}

async function validateImportedBookmarksByParent(parentGuid, expectedChildrenTotal) {
  let currentPlace = PLACES.filter((elem) => {
    return elem.guid === parentGuid.toString();
  })[0];

  let bookmarksTree = await PlacesUtils.promiseBookmarksTree(parentGuid);

  Assert.equal(bookmarksTree.children.length, expectedChildrenTotal, `Imported bookmarks length should be ${expectedChildrenTotal}`);

  for (let importedBookmark of bookmarksTree.children) {
    Assert.equal(importedBookmark.type, PlacesUtils.TYPE_X_MOZ_PLACE, `Exported bookmarks should be of type bookmark`);

    let doesTitleContain = importedBookmark.title.toString().includes(`${currentPlace.prefix} Bookmark`);
    Assert.equal(doesTitleContain, true, `Bookmark title should contain text: ${currentPlace.prefix} Bookmark`);

    let doesUriContains = importedBookmark.uri.toString().includes(BASE_URL);
    Assert.equal(doesUriContains, true, "Bookmark uri should contain base url");
  }
}

async function validateImportedBookmarks(fromPlaces) {
  for (let i = 0; i < fromPlaces.length; i++) {
    let parentContainer = fromPlaces[i];
    await validateImportedBookmarksByParent(parentContainer.guid, parentContainer.total);
  }
}

async function promiseImportExport(aWindow) {
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

add_task(async function setup() {
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

add_task(async function test_export_json() {
  let libraryWindow = await promiseLibrary();
  libraryWindow.document.querySelector("#maintenanceButtonPopup #backupBookmarks").click();

  let backupFile = await promiseImportExport();
  await BrowserTestUtils.waitForCondition(backupFile.exists);
  await promiseLibraryClosed(libraryWindow);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_import_json() {
  let libraryWindow = await promiseLibrary();
  libraryWindow.document.querySelector("#maintenanceButtonPopup #restoreFromFile").click();

  await promiseImportExport();
  await BrowserTestUtils.promiseAlertDialogOpen("accept");

  let restored = 0;
  let promiseBookmarksRestored = PlacesTestUtils.waitForNotification("onItemAdded", () => {
    restored++;
    return restored === actualBookmarks.length;
  });

  await promiseBookmarksRestored;
  await validateImportedBookmarks(PLACES);
  await promiseLibraryClosed(libraryWindow);

  registerCleanupFunction(async () => {
    if (saveDir) {
      saveDir.remove(true);
    }
  });
});
