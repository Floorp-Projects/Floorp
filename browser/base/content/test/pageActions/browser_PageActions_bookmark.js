"use strict";

add_task(async function starButtonCtrlClick() {
  // Open a unique page.
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  let url = "http://example.com/browser_page_action_star_button";
  await BrowserTestUtils.withNewTab(url, async () => {
    StarUI._createPanelIfNeeded();
    // The button ignores activation while the bookmarked status is being
    // updated. So, wait for it to finish updating.
    await TestUtils.waitForCondition(
      () => BookmarkingUI.status != BookmarkingUI.STATUS_UPDATING
    );

    const popup = document.getElementById("editBookmarkPanel");
    const starButtonBox = document.getElementById("star-button-box");

    let shownPromise = promisePanelShown(popup);
    EventUtils.synthesizeMouseAtCenter(starButtonBox, { ctrlKey: true });
    await shownPromise;
    ok(true, "Panel shown after button pressed");

    let hiddenPromise = promisePanelHidden(popup);
    document.getElementById("editBookmarkPanelRemoveButton").click();
    await hiddenPromise;
  });
});

add_task(async function bookmark() {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const url = "http://example.com/browser_page_action_menu";

  const win = await BrowserTestUtils.openNewBrowserWindow();
  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url },
    async () => {
      // The bookmark button should not be starred.
      const bookmarkButton =
        win.BrowserPageActions.urlbarButtonNodeForActionID("bookmark");
      Assert.ok(!bookmarkButton.hasAttribute("starred"));

      info("Click the button.");
      // The button ignores activation while the bookmarked status is being
      // updated. So, wait for it to finish updating.
      await TestUtils.waitForCondition(
        () => BookmarkingUI.status != BookmarkingUI.STATUS_UPDATING
      );
      const starUIPanel = win.StarUI.panel;
      let panelShown = BrowserTestUtils.waitForPopupEvent(starUIPanel, "shown");
      EventUtils.synthesizeMouseAtCenter(bookmarkButton, {}, win);
      await panelShown;
      is(
        await PlacesUtils.bookmarks.fetch({ url }),
        null,
        "Bookmark has not been created before save."
      );

      // The bookmark button should now be starred.
      Assert.equal(bookmarkButton.firstChild.getAttribute("starred"), "true");

      info("Save the bookmark.");
      const onItemAddedPromise = PlacesTestUtils.waitForNotification(
        "bookmark-added",
        events => events.some(event => event.url == url)
      );
      starUIPanel.hidePopup();
      await onItemAddedPromise;

      info("Click it again.");
      // The button ignores activation while the bookmarked status is being
      // updated. So, wait for it to finish updating.
      await TestUtils.waitForCondition(
        () => BookmarkingUI.status != BookmarkingUI.STATUS_UPDATING
      );
      panelShown = BrowserTestUtils.waitForPopupEvent(starUIPanel, "shown");
      EventUtils.synthesizeMouseAtCenter(bookmarkButton, {}, win);
      await panelShown;

      info("Remove the bookmark.");
      const onItemRemovedPromise = PlacesTestUtils.waitForNotification(
        "bookmark-removed",
        events => events.some(event => event.url == url)
      );
      win.StarUI._element("editBookmarkPanelRemoveButton").click();
      await onItemRemovedPromise;
    }
  );
});

add_task(async function bookmarkNoEditDialog() {
  const url =
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/browser_page_action_menu_no_edit_dialog";

  await SpecialPowers.pushPrefEnv({
    set: [["browser.bookmarks.editDialog.showForNewBookmarks", false]],
  });
  const win = await BrowserTestUtils.openNewBrowserWindow();
  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url },
    async () => {
      info("Click the button.");
      // The button ignores activation while the bookmarked status is being
      // updated. So, wait for it to finish updating.
      await TestUtils.waitForCondition(
        () => BookmarkingUI.status != BookmarkingUI.STATUS_UPDATING
      );
      const bookmarkButton = win.document.getElementById(
        BrowserPageActions.urlbarButtonNodeIDForActionID("bookmark")
      );

      // The bookmark should be saved immediately after clicking the star.
      const onItemAddedPromise = PlacesTestUtils.waitForNotification(
        "bookmark-added",
        events => events.some(event => event.url == url)
      );
      EventUtils.synthesizeMouseAtCenter(bookmarkButton, {}, win);
      await onItemAddedPromise;
    }
  );
});
