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

  add_task(async function test_bookmarkAllTabs_instantEditBookmark() {
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
        Assert.ok(
          !acceptBtn.disabled,
          "InstantEditBookmark: Accept button is enabled"
        );

        let namepicker = dialog.document.getElementById(
          "editBMPanel_namePicker"
        );
        Assert.ok(
          !namepicker.readOnly,
          "InstantEditBookmark: Name field is writable"
        );
        let folderName = dialog.document
          .getElementById("stringBundle")
          .getString("bookmarkAllTabsDefault");
        Assert.equal(
          namepicker.value,
          folderName,
          "InstantEditBookmark: Name field is correct."
        );

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
          "InstantEditBookmark: The folder is the expected one."
        );

        let savedItemId = dialog.gEditItemOverlay.itemId;
        let savedItemGuid = await PlacesUtils.promiseItemGuid(savedItemId);
        let entry = await PlacesUtils.bookmarks.fetch(savedItemGuid);
        Assert.equal(
          entry.parentGuid,
          defaultParentGuid,
          "InstantEditBookmark: Should have created the keyword in the right folder."
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

  add_task(async function test_bookmarkAllTabs_eitBookmark() {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.bookmarks.editDialog.delayedApply.enabled", true]],
    });
    await withBookmarksDialog(
      false,
      function open() {
        document.getElementById("Browser:BookmarkAllTabs").doCommand();
      },
      async dialog => {
        let acceptBtn = dialog.document
          .getElementById("bookmarkpropertiesdialog")
          .getButton("accept");
        Assert.ok(
          !acceptBtn.disabled,
          "EditBookmark: Accept button is enabled"
        );

        let namepicker = dialog.document.getElementById(
          "editBMPanel_namePicker"
        );
        Assert.ok(!namepicker.readOnly, "EditBookmark: Name field is writable");
        let folderName = dialog.document
          .getElementById("stringBundle")
          .getString("bookmarkAllTabsDefault");
        Assert.equal(
          namepicker.value,
          folderName,
          "EditBookmark: Name field is correct."
        );

        fillBookmarkTextField("editBMPanel_namePicker", "folder", dialog);

        let folderPicker = dialog.document.getElementById(
          "editBMPanel_folderMenuList"
        );

        let defaultParentGuid = await PlacesUIUtils.defaultParentGuid;
        // Check the initial state of the folder picker.
        await TestUtils.waitForCondition(
          () => folderPicker.getAttribute("selectedGuid") == defaultParentGuid,
          "EditBookmark: The folder is the expected one."
        );

        let savedItemId = dialog.gEditItemOverlay.itemId;
        let savedItemGuid = await PlacesUtils.promiseItemGuid(savedItemId);
        let entry = await PlacesUtils.bookmarks.fetch(savedItemGuid);
        Assert.equal(
          entry.parentGuid,
          defaultParentGuid,
          "EditBookmark: Should have created the keyword in the right folder."
        );

        EventUtils.synthesizeKey("VK_RETURN", {}, dialog);
      }
    );

    let bookmark1 = await PlacesUtils.bookmarks.fetch({ url: "about:mozilla" });
    let bookmark2 = await PlacesUtils.bookmarks.fetch({ url: "about:robots" });
    let defaultParentGuid = await PlacesUIUtils.defaultParentGuid;

    Assert.equal(
      bookmark1.parentGuid,
      bookmark2.parentGuid,
      "EditBookmark: Both bookmarks created in same folder"
    );
    let folder = await PlacesUtils.bookmarks.fetch(bookmark1.parentGuid);
    Assert.equal(
      folder.parentGuid,
      defaultParentGuid,
      "EditBookmark: ParentGuid of bokmarks is default one as expected (toolbar)"
    );
    Assert.equal(
      folder.title,
      "folder",
      "EditBookmark: folder title is expected one"
    );

    await PlacesUtils.bookmarks.eraseEverything();
  });
});
