/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test that the bookmark location (url) can be changed from the toolbar and the sidebar.
 */
"use strict";

const TEST_URL = "about:buildconfig";
const TEST_URL2 = "about:config";
const TEST_URL3 = "about:credits";

// Setup.
add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.visibility", "always"]],
  });

  // The following initialization code is necessary to avoid a frequent
  // intermittent failure in verify-fission where, due to timings, we may or
  // may not import default bookmarks. We also want to avoid the empty toolbar
  // placeholder shifting stuff around.
  info("Ensure Places init is complete");
  let placesInitCompleteObserved = TestUtils.topicObserved(
    "places-browser-init-complete"
  );
  Cc["@mozilla.org/browser/browserglue;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "browser-glue-test", "places-browser-init-complete");
  await placesInitCompleteObserved;
  info("Add a bookmark to avoid showing the empty toolbar placeholder.");
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "initial",
    url: TEST_URL,
  });

  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;
  if (wasCollapsed) {
    info("Show the bookmarks toolbar");
    await promiseSetToolbarVisibility(toolbar, true);
    info("Ensure toolbar visibility was updated");
    await BrowserTestUtils.waitForEvent(
      toolbar,
      "BookmarksToolbarVisibilityUpdated"
    );
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

add_task(async function test_change_location_from_Toolbar() {
  let toolbarBookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "",
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
      placesContext.activateItem(properties);
    },
    async function test(dialogWin) {
      // Check the initial location.
      let locationPicker = dialogWin.document.getElementById(
        "editBMPanel_locationField"
      );
      Assert.equal(
        locationPicker.value,
        TEST_URL,
        "The location is the expected one."
      );

      let promiseLocationChange = PlacesTestUtils.waitForNotification(
        "onItemChanged",
        (id, parentId, index, itemUrl) => itemUrl === TEST_URL2
      );
      // Update the "location" field.
      fillBookmarkTextField(
        "editBMPanel_locationField",
        TEST_URL2,
        dialogWin,
        false
      );
      await waitForCondition(
        () => locationPicker.value === TEST_URL2,
        "The location is correct after update."
      );
      locationPicker.blur();
      await promiseLocationChange;
      Assert.equal(
        dialogWin.gEditItemOverlay.uri.spec,
        TEST_URL2,
        "The location is the expected one."
      );
      locationPicker.focus();
      // Confirm and close the dialog.
      EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);

      let updatedBm = await PlacesUtils.bookmarks.fetch(toolbarBookmark.guid);
      Assert.equal(
        updatedBm.url,
        TEST_URL2,
        "Should have updated the bookmark location in the database."
      );
    }
  );
});

add_task(async function test_change_location_from_Sidebar() {
  let bm = await PlacesUtils.bookmarks.fetch({ url: TEST_URL2 });

  await withSidebarTree("bookmarks", async function(tree) {
    tree.selectItems([bm.guid]);

    await withBookmarksDialog(
      false,
      function openPropertiesDialog() {
        tree.controller.doCommand("placesCmd_show:info");
      },
      async function test(dialogWin) {
        // Check the initial location.
        let locationPicker = dialogWin.document.getElementById(
          "editBMPanel_locationField"
        );
        Assert.equal(
          locationPicker.value,
          TEST_URL2,
          "The location is the expected one."
        );

        let promiseLocationChange = PlacesTestUtils.waitForNotification(
          "onItemChanged",
          (id, parentId, index, itemUrl) => itemUrl === TEST_URL3
        );

        // Update the "location" field.
        fillBookmarkTextField(
          "editBMPanel_locationField",
          TEST_URL3,
          dialogWin,
          false
        );
        await waitForCondition(
          () => locationPicker.value === TEST_URL3,
          "The location is correct after update."
        );

        // Confirm and close the dialog.
        EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
        await promiseLocationChange;

        let updatedBm = await PlacesUtils.bookmarks.fetch(bm.guid);
        Assert.equal(
          updatedBm.url,
          TEST_URL3,
          "Should have updated the bookmark location in the database."
        );
      }
    );
  });
});
