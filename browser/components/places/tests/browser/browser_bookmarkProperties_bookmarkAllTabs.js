"use strict";

const TEST_URLS = ["about:robots", "about:mozilla"];

add_task(async function() {
  let tabs = [];
  for (let url of TEST_URLS) {
    tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, url));
  }
  registerCleanupFunction(async function() {
    for (let tab of tabs) {
      BrowserTestUtils.removeTab(tab);
    }
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
