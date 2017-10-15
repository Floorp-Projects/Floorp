"use strict";

const TEST_URLS = [
  "about:robots",
  "about:mozilla",
];

add_task(async function() {
  let tabs = [];
  for (let url of TEST_URLS) {
    tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, url));
  }
  registerCleanupFunction(async function() {
    for (let tab of tabs) {
      await BrowserTestUtils.removeTab(tab);
    }
  });

  await withBookmarksDialog(true,
    function open() {
      document.getElementById("Browser:BookmarkAllTabs").doCommand();
    },
    async dialog => {
      let acceptBtn = dialog.document.documentElement.getButton("accept");
      ok(!acceptBtn.disabled, "Accept button is enabled");

      let namepicker = dialog.document.getElementById("editBMPanel_namePicker");
      Assert.ok(!namepicker.readOnly, "Name field is writable");
      let folderName = dialog.document.getElementById("stringBundle").getString("bookmarkAllTabsDefault");
      Assert.equal(namepicker.value, folderName, "Name field is correct.");

      let promiseTitleChange = promiseBookmarksNotification(
        "onItemChanged", (id, prop, isAnno, val) => prop == "title" && val == "folder");
      fillBookmarkTextField("editBMPanel_namePicker", "folder", dialog);
      await promiseTitleChange;
    },
    dialog => {
      let savedItemId = dialog.gEditItemOverlay.itemId;
      ok(savedItemId > 0, "Found the itemId");
      return PlacesTestUtils.waitForNotification("onItemRemoved",
                                                 id => id === savedItemId);
    }
  );
});
