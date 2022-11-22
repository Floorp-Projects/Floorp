"use strict";

const TEST_URLS = ["about:robots", "about:mozilla"];
const TEST_FOLDER_NAME = "folder";

add_task(async function bookmark_all_tabs_using_instant_apply() {
  let tabs = [];
  for (let url of TEST_URLS) {
    tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, url));
  }
  registerCleanupFunction(async function() {
    for (let tab of tabs) {
      BrowserTestUtils.removeTab(tab);
    }
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.bookmarks.editDialog.delayedApply.enabled", false]],
  });
  await withBookmarksDialog(
    true,
    function open() {
      document.getElementById("Browser:BookmarkAllTabs").doCommand();
    },
    async dialog => {
      let acceptBtn = dialog.document
        .getElementById("bookmarkpropertiesdialog")
        .getButton("accept");
      Assert.ok(!acceptBtn.disabled, "Accept button is enabled");

      let namepicker = dialog.document.getElementById("editBMPanel_namePicker");
      Assert.ok(!namepicker.readOnly, "Name field is writable");
      let folderName = dialog.document
        .getElementById("stringBundle")
        .getString("bookmarkAllTabsDefault");
      Assert.equal(namepicker.value, folderName, "Name field is correct.");

      let promiseTitleChange = PlacesTestUtils.waitForNotification(
        "bookmark-title-changed",
        events => events.some(e => e.title === "folder"),
        "places"
      );

      fillBookmarkTextField("editBMPanel_namePicker", "folder", dialog);
      await promiseTitleChange;

      let folderPicker = dialog.document.getElementById(
        "editBMPanel_folderMenuList"
      );

      let defaultParentGuid = await PlacesUIUtils.defaultParentGuid;
      // Check the initial state of the folder picker.
      await TestUtils.waitForCondition(
        () => folderPicker.getAttribute("selectedGuid") == defaultParentGuid,
        "The folder is the expected one."
      );

      let savedItemId = dialog.gEditItemOverlay.itemId;
      let savedItemGuid = await PlacesUtils.promiseItemGuid(savedItemId);
      let entry = await PlacesUtils.bookmarks.fetch(savedItemGuid);
      Assert.equal(
        entry.parentGuid,
        defaultParentGuid,
        "Should have created the keyword in the right folder."
      );
    },
    dialog => {
      let savedItemId = dialog.gEditItemOverlay.itemId;
      Assert.greater(savedItemId, 0, "Found the itemId");
      return PlacesTestUtils.waitForNotification(
        "bookmark-removed",
        events => events.some(event => event.id === savedItemId),
        "places"
      );
    }
  );
});

add_task(async function bookmark_all_tabs_using_delayed_apply() {
  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.bookmarks.editDialog.delayedApply.enabled", true]],
  });
  await withBookmarksDialog(
    false,
    function open() {
      document.getElementById("Browser:BookmarkAllTabs").doCommand();
    },
    async dialog => {
      for (const url of TEST_URLS) {
        is(
          await PlacesUtils.bookmarks.fetch({ url }),
          null,
          "Bookmark has not been created before accepting dialog box."
        );
      }

      fillBookmarkTextField("editBMPanel_namePicker", TEST_FOLDER_NAME, dialog);
      const bookmarkAdded = PlacesTestUtils.waitForNotification(
        "bookmark-added",
        undefined,
        "places"
      );
      EventUtils.synthesizeKey("VK_RETURN", {}, dialog);
      await bookmarkAdded;

      for (const url of TEST_URLS) {
        const { parentGuid } = await PlacesUtils.bookmarks.fetch({ url });
        const folder = await PlacesUtils.bookmarks.fetch({
          guid: parentGuid,
        });
        is(
          folder.title,
          TEST_FOLDER_NAME,
          "Bookmark created in the right folder."
        );
      }
    }
  );
});
