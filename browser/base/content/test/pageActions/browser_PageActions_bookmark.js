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
  // Open a unique page.
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  let url = "http://example.com/browser_page_action_menu";
  await BrowserTestUtils.withNewTab(url, async () => {
    // The bookmark button should read "Bookmark this page ([shortcut])" and not
    // be starred.
    let bookmarkButton = BrowserPageActions.urlbarButtonNodeForActionID(
      "bookmark"
    );
    let tooltipText = bookmarkButton.getAttribute("tooltiptext");
    Assert.ok(
      tooltipText.startsWith("Bookmark this page"),
      `Expecting the tooltip text to be updated. Tooltip text: ${tooltipText}`
    );
    Assert.ok(!bookmarkButton.hasAttribute("starred"));

    info("Click the button.");
    // The button ignores activation while the bookmarked status is being
    // updated. So, wait for it to finish updating.
    await TestUtils.waitForCondition(
      () => BookmarkingUI.status != BookmarkingUI.STATUS_UPDATING
    );
    let onItemAddedPromise = PlacesTestUtils.waitForNotification(
      "bookmark-added",
      events => events.some(event => event.url == url),
      "places"
    );
    let promise = BrowserTestUtils.waitForPopupEvent(StarUI.panel, "shown");
    EventUtils.synthesizeMouseAtCenter(bookmarkButton, {});
    await promise;
    await onItemAddedPromise;

    Assert.equal(
      BookmarkingUI.starBox.getAttribute("open"),
      "true",
      "Star has open attribute"
    );
    // The bookmark button should now read "Edit this bookmark ([shortcut])" and
    // be starred.
    tooltipText = bookmarkButton.getAttribute("tooltiptext");
    Assert.ok(
      tooltipText.startsWith("Edit this bookmark"),
      `Expecting the tooltip text to be updated. Tooltip text: ${tooltipText}`
    );
    Assert.equal(bookmarkButton.firstChild.getAttribute("starred"), "true");

    StarUI.panel.hidePopup();
    Assert.ok(
      !BookmarkingUI.starBox.hasAttribute("open"),
      "Star no longer has open attribute"
    );

    info("Click it again.");
    // The button ignores activation while the bookmarked status is being
    // updated. So, wait for it to finish updating.
    await TestUtils.waitForCondition(
      () => BookmarkingUI.status != BookmarkingUI.STATUS_UPDATING
    );
    promise = BrowserTestUtils.waitForPopupEvent(StarUI.panel, "shown");
    EventUtils.synthesizeMouseAtCenter(bookmarkButton, {});
    await promise;

    let onItemRemovedPromise = PlacesTestUtils.waitForNotification(
      "bookmark-removed",
      events => events.some(event => event.url == url),
      "places"
    );
    // Click the remove-bookmark button in the panel.
    StarUI._element("editBookmarkPanelRemoveButton").click();
    // Wait for the bookmark to be removed before continuing.
    await onItemRemovedPromise;

    // Check there's no contextual menu on the button.
    let contextMenuPromise = promisePopupNotShown("pageActionContextMenu");
    // The button ignores activation while the bookmarked status is being
    // updated. So, wait for it to finish updating.
    await TestUtils.waitForCondition(
      () => BookmarkingUI.status != BookmarkingUI.STATUS_UPDATING
    );
    EventUtils.synthesizeMouseAtCenter(bookmarkButton, {
      type: "contextmenu",
      button: 2,
    });
    await contextMenuPromise;
  });
});

add_task(async function bookmarkDelayedApply() {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const url = "http://example.com/browser_page_action_menu_delayed_apply";

  // Open a new window with delayed apply mode enabled. In the new window,
  // opening the star panel should delay creating a new bookmark until the
  // panel is dismissed.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.bookmarks.editDialog.delayedApply.enabled", true]],
  });
  const win = await BrowserTestUtils.openNewBrowserWindow();
  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url },
    async () => {
      // The bookmark button should not be starred.
      const bookmarkButton = win.BrowserPageActions.urlbarButtonNodeForActionID(
        "bookmark"
      );
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
        events => events.some(event => event.url == url),
        "places"
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
        events => events.some(event => event.url == url),
        "places"
      );
      win.StarUI._element("editBookmarkPanelRemoveButton").click();
      await onItemRemovedPromise;
    }
  );
});

add_task(async function bookmarkDelayedApplyNoEditDialog() {
  const url =
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/browser_page_action_menu_delayed_apply_no_edit_dialog";

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.bookmarks.editDialog.delayedApply.enabled", true],
      ["browser.bookmarks.editDialog.showForNewBookmarks", false],
    ],
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
        events => events.some(event => event.url == url),
        "places"
      );
      EventUtils.synthesizeMouseAtCenter(bookmarkButton, {}, win);
      await onItemAddedPromise;
    }
  );
});
