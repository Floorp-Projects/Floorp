/**
 * Tests that the title of a bookmark can be changed from the bookmark star, toolbar, and sidebar.
 */
"use strict";

const TEST_URL = "about:buildconfig";
const titleAfterFirstUpdate = "BookmarkStar title";

function getToolbarNodeForItemGuid(aItemGuid) {
  var children = document.getElementById("PlacesToolbarItems").children;
  for (let child of children) {
    if (aItemGuid == child._placesNode.bookmarkGuid) {
      return child;
    }
  }
  return null;
}

// Setup.
add_task(async function setup() {
  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;

  // Uncollapse the personal toolbar if needed.
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
  }

  registerCleanupFunction(async () => {
    // Collapse the personal toolbar if needed.
    if (wasCollapsed) {
      await promiseSetToolbarVisibility(toolbar, false);
    }
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_change_title_from_BookmarkStar() {
  let originalBm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: "Before Edit",
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_URL,
    waitForStateStop: true,
  });

  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
  });

  StarUI._createPanelIfNeeded();
  let bookmarkPanel = document.getElementById("editBookmarkPanel");
  let shownPromise = promisePopupShown(bookmarkPanel);

  let bookmarkStar = BookmarkingUI.star;
  bookmarkStar.click();

  await shownPromise;

  let bookmarkPanelTitle = document.getElementById("editBookmarkPanelTitle");
  Assert.equal(
    bookmarkPanelTitle.textContent,
    gFluentStrings.formatValueSync("bookmarks-edit-bookmark"),
    "Bookmark title is correct"
  );

  let promiseNotification = PlacesTestUtils.waitForNotification(
    "onItemChanged",
    (id, property, isAnno, val) =>
      property == "title" && val == titleAfterFirstUpdate
  );

  // Update the bookmark's title.
  await fillBookmarkTextField(
    "editBMPanel_namePicker",
    titleAfterFirstUpdate,
    window
  );

  let doneButton = document.getElementById("editBookmarkPanelDoneButton");
  doneButton.click();
  await promiseNotification;

  let updatedBm = await PlacesUtils.bookmarks.fetch(originalBm.guid);
  Assert.equal(
    updatedBm.title,
    titleAfterFirstUpdate,
    "Should have updated the bookmark title in the database"
  );
});

add_task(async function test_change_title_from_Toolbar() {
  let toolbarBookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: titleAfterFirstUpdate,
    url: TEST_URL,
  });

  let toolbarNode = getToolbarNodeForItemGuid(toolbarBookmark.guid);

  await withBookmarksDialog(
    false,
    async function openPropertiesDialog() {
      let placesContext = document.getElementById("placesContext");
      let promisePopup = BrowserTestUtils.waitForEvent(
        placesContext,
        "popupshown"
      );
      EventUtils.synthesizeMouseAtCenter(toolbarNode, {
        button: 2,
        type: "contextmenu",
      });
      await promisePopup;

      let properties = document.getElementById(
        "placesContext_show_bookmark:info"
      );
      placesContext.activateItem(properties, {});
    },
    async function test(dialogWin) {
      // Ensure the dialog has initialized.
      await TestUtils.waitForCondition(() => dialogWin.document.title);

      let namepicker = dialogWin.document.getElementById(
        "editBMPanel_namePicker"
      );

      let editBookmarkDialogTitle = dialogWin.document.getElementById(
        "titleText"
      );
      let bundle = dialogWin.document.getElementById("stringBundle");

      Assert.equal(
        bundle.getString("dialogTitleEditBookmark2"),
        editBookmarkDialogTitle.textContent
      );

      Assert.equal(
        namepicker.value,
        titleAfterFirstUpdate,
        "Name field is correct before update."
      );

      let promiseTitleChange = PlacesTestUtils.waitForNotification(
        "onItemChanged",
        (id, prop, isAnno, val) => prop == "title" && val == "Toolbar title"
      );

      // Update the bookmark's title.
      fillBookmarkTextField(
        "editBMPanel_namePicker",
        "Toolbar title",
        dialogWin,
        false
      );

      // Confirm and close the dialog.
      EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
      await promiseTitleChange;

      let updatedBm = await PlacesUtils.bookmarks.fetch(toolbarBookmark.guid);
      Assert.equal(
        updatedBm.title,
        "Toolbar title",
        "Should have updated the bookmark title in the database"
      );
    }
  );
});

add_task(async function test_change_title_from_Sidebar() {
  let bookmarks = [];
  await PlacesUtils.bookmarks.fetch({ url: TEST_URL }, bm =>
    bookmarks.push(bm)
  );

  await withSidebarTree("bookmarks", async function(tree) {
    tree.selectItems([bookmarks[0].guid]);

    await withBookmarksDialog(
      false,
      function openPropertiesDialog() {
        tree.controller.doCommand("placesCmd_show:info");
      },
      async function test(dialogWin) {
        let namepicker = dialogWin.document.getElementById(
          "editBMPanel_namePicker"
        );
        Assert.equal(
          namepicker.value,
          "Toolbar title",
          "Name field is correct before update."
        );

        let promiseTitleChange = PlacesTestUtils.waitForNotification(
          "onItemChanged",
          (id, prop, isAnno, val) => prop == "title" && val == "Sidebar Title"
        );

        // Update the bookmark's title.
        fillBookmarkTextField(
          "editBMPanel_namePicker",
          "Sidebar Title",
          dialogWin,
          false
        );

        // Confirm and close the dialog.
        EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
        await promiseTitleChange;

        // Get updated bookmarks, check the new title.
        bookmarks = [];
        await PlacesUtils.bookmarks.fetch({ url: TEST_URL }, bm =>
          bookmarks.push(bm)
        );
        Assert.equal(
          bookmarks[0].title,
          "Sidebar Title",
          "Should have updated the bookmark title in the database"
        );
        Assert.equal(bookmarks.length, 2, "Two bookmarks should exist");
      }
    );
  });
});
