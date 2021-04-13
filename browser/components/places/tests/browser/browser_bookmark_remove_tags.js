/**
 * Tests that the bookmark tags can be removed from the bookmark star, toolbar and sidebar.
 */
"use strict";

const TEST_URL = "about:buildconfig";
const TEST_URI = Services.io.newURI(TEST_URL);

// Setup.
add_task(async function setup() {
  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;

  // Uncollapse the personal toolbar if needed.
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
  }

  // Cleanup.
  registerCleanupFunction(async () => {
    // Collapse the personal toolbar if needed.
    if (wasCollapsed) {
      await promiseSetToolbarVisibility(toolbar, false);
    }
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_remove_tags_from_BookmarkStar() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: TEST_URL,
  });
  PlacesUtils.tagging.tagURI(TEST_URI, ["tag1, tag2, tag3, tag4"]);

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

  // Check if the "Edit This Bookmark" panel is open.
  let bookmarkPanelTitle = document.getElementById("editBookmarkPanelTitle");
  Assert.equal(
    bookmarkPanelTitle.textContent,
    gFluentStrings.formatValueSync("bookmarks-edit-bookmark"),
    "Bookmark panel title is correct."
  );

  let promiseTagsChange = PlacesTestUtils.waitForNotification(
    "onItemChanged",
    (id, property) => property === "tags"
  );

  // Update the "tags" field.
  fillBookmarkTextField("editBMPanel_tagsField", "tag1, tag2, tag3", window);
  let tagspicker = document.getElementById("editBMPanel_tagsField");
  await waitForCondition(
    () => tagspicker.value === "tag1, tag2, tag3",
    "Tags are correct after update."
  );

  let doneButton = document.getElementById("editBookmarkPanelDoneButton");
  doneButton.click();
  await promiseTagsChange;

  let tags = PlacesUtils.tagging.getTagsForURI(TEST_URI);
  Assert.deepEqual(
    tags,
    ["tag1", "tag2", "tag3"],
    "Should have updated the bookmark tags in the database."
  );
});

add_task(async function test_remove_tags_from_Toolbar() {
  let toolbarBookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: TEST_URL,
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
      let tagspicker = dialogWin.document.getElementById(
        "editBMPanel_tagsField"
      );
      Assert.equal(
        tagspicker.value,
        "tag1, tag2, tag3",
        "Tags are correct before update."
      );

      let promiseTagsChange = PlacesTestUtils.waitForNotification(
        "onItemChanged",
        (id, property) => property === "tags"
      );

      // Update the "tags" field.
      fillBookmarkTextField(
        "editBMPanel_tagsField",
        "tag1, tag2",
        dialogWin,
        false
      );
      await waitForCondition(
        () => tagspicker.value === "tag1, tag2",
        "Tags are correct after update."
      );

      // Confirm and close the dialog.
      EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
      await promiseTagsChange;

      let tags = PlacesUtils.tagging.getTagsForURI(TEST_URI);
      Assert.deepEqual(
        tags,
        ["tag1", "tag2"],
        "Should have updated the bookmark tags in the database."
      );
    }
  );
});

add_task(async function test_remove_tags_from_Sidebar() {
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
        let tagspicker = dialogWin.document.getElementById(
          "editBMPanel_tagsField"
        );
        Assert.equal(
          tagspicker.value,
          "tag1, tag2",
          "Tags are correct before update."
        );

        let promiseTagsChange = PlacesTestUtils.waitForNotification(
          "onItemChanged",
          (id, property) => property === "tags"
        );

        // Update the "tags" field.
        fillBookmarkTextField(
          "editBMPanel_tagsField",
          "tag1",
          dialogWin,
          false
        );
        await waitForCondition(
          () => tagspicker.value === "tag1",
          "Tags are correct after update."
        );

        // Confirm and close the dialog.
        EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
        await promiseTagsChange;

        let tags = PlacesUtils.tagging.getTagsForURI(TEST_URI);
        Assert.deepEqual(
          tags,
          ["tag1"],
          "Should have updated the bookmark tags in the database."
        );
      }
    );
  });
});
