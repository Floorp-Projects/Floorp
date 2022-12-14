/**
 * Tests that the add bookmark for frame dialog functions correctly.
 */

const BASE_URL =
  "http://mochi.test:8888/browser/browser/components/places/tests/browser";
const PAGE_URL = BASE_URL + "/framedPage.html";
const LEFT_URL = BASE_URL + "/frameLeft.html";
const RIGHT_URL = BASE_URL + "/frameRight.html";

async function withAddBookmarkForFrame(taskFn) {
  // Open a tab and wait for all the subframes to load.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);

  let contentAreaContextMenu = document.getElementById(
    "contentAreaContextMenu"
  );

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#left",
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
  await popupShownPromise;

  await withBookmarksDialog(
    true,
    function() {
      let frameMenuItem = document.getElementById("frame");
      frameMenuItem.click();

      let bookmarkFrame = document.getElementById("context-bookmarkframe");
      bookmarkFrame.click();
      contentAreaContextMenu.hidePopup();
    },
    taskFn
  );

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_open_add_bookmark_for_frame() {
  info("Test basic opening of the add bookmark for frame dialog.");
  await withAddBookmarkForFrame(async dialogWin => {
    let namepicker = dialogWin.document.getElementById(
      "editBMPanel_namePicker"
    );
    Assert.ok(!namepicker.readOnly, "Name field is writable");
    Assert.equal(namepicker.value, "Left frame", "Name field is correct.");

    let expectedFolder = "BookmarksToolbarFolderTitle";
    let expectedFolderName = PlacesUtils.getString(expectedFolder);

    let folderPicker = dialogWin.document.getElementById(
      "editBMPanel_folderMenuList"
    );

    await TestUtils.waitForCondition(
      () => folderPicker.selectedItem.label == expectedFolderName,
      "Dialog: The folder is the expected one."
    );

    let tagsField = dialogWin.document.getElementById("editBMPanel_tagsField");
    Assert.equal(tagsField.value, "", "Dialog: The tags field should be empty");
  });
});

add_task(
  async function test_move_bookmark_whilst_add_bookmark_open_instantEditBookmark() {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.bookmarks.editDialog.delayedApply.enabled", false]],
    });
    info(
      "InstantEditBookmark: Test moving a bookmark whilst the add bookmark for frame dialog is open."
    );
    await withAddBookmarkForFrame(async dialogWin => {
      let expectedGuid = await PlacesUIUtils.defaultParentGuid;
      let expectedFolder = "BookmarksToolbarFolderTitle";
      let expectedFolderName = PlacesUtils.getString(expectedFolder);
      let bookmarksMenuFolderName = PlacesUtils.getString(
        "BookmarksMenuFolderTitle"
      );

      let url = makeURI(LEFT_URL);
      let folderPicker = dialogWin.document.getElementById(
        "editBMPanel_folderMenuList"
      );

      // Check the initial state of the folder picker.
      await TestUtils.waitForCondition(
        () => folderPicker.selectedItem.label == expectedFolderName,
        "InstantEditBookmark: The folder is the expected one."
      );

      // Check the bookmark has been created as expected.
      let bookmark = await PlacesUtils.bookmarks.fetch({ url });

      Assert.equal(
        bookmark.parentGuid,
        expectedGuid,
        "InstantEditBookmark: The bookmark should be in the expected folder."
      );

      // Now move the bookmark and check the folder picker is updated correctly.
      bookmark.parentGuid = PlacesUtils.bookmarks.menuGuid;
      bookmark.index = PlacesUtils.bookmarks.DEFAULT_INDEX;

      await PlacesUtils.bookmarks.update(bookmark);

      await TestUtils.waitForCondition(
        () => folderPicker.selectedItem.label == bookmarksMenuFolderName,
        "InstantEditBookmark: The folder picker has changed to the new folder"
      );
    });
  }
);

add_task(
  async function test_move_bookmark_whilst_add_bookmark_open_editBookmakr() {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.bookmarks.editDialog.delayedApply.enabled", true]],
    });
    info(
      "EditBookmark: Test moving a bookmark whilst the add bookmark for frame dialog is open."
    );
    await PlacesUtils.bookmarks.eraseEverything();
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);

    let contentAreaContextMenu = document.getElementById(
      "contentAreaContextMenu"
    );

    let popupShownPromise = BrowserTestUtils.waitForEvent(
      contentAreaContextMenu,
      "popupshown"
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#left",
      { type: "contextmenu", button: 2 },
      gBrowser.selectedBrowser
    );
    await popupShownPromise;

    await withBookmarksDialog(
      false,
      function() {
        let frameMenuItem = document.getElementById("frame");
        frameMenuItem.click();

        let bookmarkFrame = document.getElementById("context-bookmarkframe");
        bookmarkFrame.click();
        contentAreaContextMenu.hidePopup();
      },
      async function(dialogWin) {
        let expectedGuid = await PlacesUIUtils.defaultParentGuid;
        let expectedFolder = "BookmarksToolbarFolderTitle";
        let expectedFolderName = PlacesUtils.getString(expectedFolder);

        let folderPicker = dialogWin.document.getElementById(
          "editBMPanel_folderMenuList"
        );

        Assert.equal(
          folderPicker.selectedItem.label,
          expectedFolderName,
          "EditBookmark: The folder is the expected one."
        );

        Assert.equal(
          folderPicker.getAttribute("selectedGuid"),
          expectedGuid,
          "EditBookmark: Should have the correct default guid selected"
        );

        dialogWin.document
          .getElementById("editBMPanel_foldersExpander")
          .click();
        let folderTree = dialogWin.document.getElementById(
          "editBMPanel_folderTree"
        );
        folderTree.selectItems([PlacesUtils.bookmarks.menuGuid]);
        folderTree.blur();

        EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
      }
    );
    let url = makeURI(LEFT_URL);
    // Check the bookmark has been moved as expected.
    let bookmark = await PlacesUtils.bookmarks.fetch({ url });

    Assert.equal(
      bookmark.parentGuid,
      PlacesUtils.bookmarks.menuGuid,
      "EditBookmark: The bookmark should be moved to the expected folder."
    );

    BrowserTestUtils.removeTab(tab);
    await PlacesUtils.bookmarks.eraseEverything();
  }
);
