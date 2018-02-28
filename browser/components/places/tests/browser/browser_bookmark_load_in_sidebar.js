/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test that a bookmark can be loaded inside the Bookmarks sidebar.
 */
"use strict";

const TEST_URL = "about:buildconfig";

// Cleanup.
registerCleanupFunction(async () => {
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_load_in_sidebar() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: TEST_URL,
  });

  await withSidebarTree("bookmarks", async function(tree) {
    tree.selectItems([bm.guid]);
    await withBookmarksDialog(
      false,
      function openPropertiesDialog() {
        tree.controller.doCommand("placesCmd_show:info");
      },

      // Check the "Load this bookmark in the sidebar" option.
      async function test(dialogWin) {
        let loadInSidebar = dialogWin.document.getElementById("editBMPanel_loadInSidebarCheckbox");
        let promiseCheckboxChanged = PlacesTestUtils.waitForNotification(
          "onItemChanged",
          (id, parentId, checked) => checked === true
        );

        loadInSidebar.click();

        EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
        await promiseCheckboxChanged;
      }
    );

    let sidebar = document.getElementById("sidebar");

    let sidebarLoadedPromise = new Promise(resolve => {
      sidebar.addEventListener("load", function() {
        executeSoon(resolve);
      }, {capture: true, once: true});
    });

    // Select and open the bookmark in the sidebar.
    tree.selectItems([bm.guid]);
    tree.controller.doCommand("placesCmd_open");

    await sidebarLoadedPromise;

    let sidebarTitle = document.getElementById("sidebar-title");
    let sidebarBrowser = sidebar.contentDocument.getElementById("web-panels-browser");

    await BrowserTestUtils.browserLoaded(sidebarBrowser, false, TEST_URL);

    let h1Elem = sidebarBrowser.contentDocument.getElementsByTagName("h1")[0];

    // Check that the title and the content of the page are loaded successfully.
    Assert.equal(sidebarTitle.value, TEST_URL, "The sidebar title is successfully loaded.");
    Assert.equal(h1Elem.textContent, TEST_URL, "The sidebar content is successfully loaded.");
  });
});
