/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF_LOAD_BOOKMARKS_IN_TABS = "browser.tabs.loadBookmarksInTabs";

var gBms;

add_task(async function setup() {
  gBms = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      title: "bm1",
      url: "about:buildconfig"
    }, {
      title: "bm2",
      url: "about:mozilla",
    }]
  });

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_open_bookmark_from_sidebar() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await withSidebarTree("bookmarks", async (tree) => {
    tree.selectItems([gBms[0].guid]);

    let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser,
      false, gBms[0].url
    );

    tree.controller.doCommand("placesCmd_open");

    await loadedPromise;

    // An assert to make the test happy.
    Assert.ok(true, "The bookmark was loaded successfully.");
  });

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_open_bookmark_from_sidebar_keypress() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await withSidebarTree("bookmarks", async (tree) => {
    tree.selectItems([gBms[1].guid]);

    let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser,
      false, gBms[1].url
    );

    tree.focus();
    EventUtils.sendKey("return");

    await loadedPromise;

    // An assert to make the test happy.
    Assert.ok(true, "The bookmark was loaded successfully.");
  });

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_open_bookmark_in_tab_from_sidebar() {
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_LOAD_BOOKMARKS_IN_TABS, true]
  ]});

  await BrowserTestUtils.withNewTab({gBrowser}, async (initialTab) => {
    await withSidebarTree("bookmarks", async (tree) => {
      tree.selectItems([gBms[0].guid]);
      let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser,
        false, gBms[0].url
      );
      tree.focus();
      EventUtils.sendKey("return");
      await loadedPromise;
      Assert.ok(true, "The bookmark reused the empty tab.");

      tree.selectItems([gBms[1].guid]);
      let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, gBms[1].url);
      tree.focus();
      EventUtils.sendKey("return");
      let newTab = await newTabPromise;
      Assert.ok(true, "The bookmark was opened in a new tab.");
      BrowserTestUtils.removeTab(newTab);
    });
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_open_bookmark_folder_from_sidebar() {
  await withSidebarTree("bookmarks", async (tree) => {
    tree.selectItems([PlacesUtils.bookmarks.virtualUnfiledGuid]);

    Assert.equal(tree.view.selection.getRangeCount(), 1,
      "Should only have one range selected");

    let loadedPromises = [];

    for (let bm of gBms) {
      loadedPromises.push(BrowserTestUtils.waitForNewTab(gBrowser,
        bm.url, false, true));
    }

    synthesizeClickOnSelectedTreeCell(tree, {button: 1});

   let tabs = await Promise.all(loadedPromises);

    for (let tab of tabs) {
      await BrowserTestUtils.removeTab(tab);
    }
  });
});
