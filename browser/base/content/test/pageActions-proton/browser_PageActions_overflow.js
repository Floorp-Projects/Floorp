/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test() {
  // We use an extension that shows a page action. We must add this additional
  // action because otherwise the meatball menu would not appear as an overflow
  // for a single action.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test contextMenu",
      page_action: { show_matches: ["<all_urls>"] },
    },

    useAddonManager: "temporary",
  });

  await extension.startup();
  let actionId = ExtensionCommon.makeWidgetId(extension.id);

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(win);
  Assert.greater(win.outerWidth, 700, "window is bigger than 700px");
  BrowserTestUtils.loadURI(win.gBrowser, "data:text/html,<h1>A Page</h1>");
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);

  // The pageAction implementation enables the button at the next animation
  // frame, so before we look for the button we should wait one animation frame
  // as well.
  await promiseAnimationFrame(win);

  info("Check page action buttons are visible, the meatball button is not");
  let addonButton = win.BrowserPageActions.urlbarButtonNodeForActionID(
    actionId
  );
  Assert.ok(BrowserTestUtils.is_visible(addonButton));
  let starButton = win.BrowserPageActions.urlbarButtonNodeForActionID(
    "bookmark"
  );
  Assert.ok(BrowserTestUtils.is_visible(starButton));
  let meatballButton = win.document.getElementById("pageActionButton");
  Assert.ok(!BrowserTestUtils.is_visible(meatballButton));

  info(
    "Shrink the window, check page action buttons are not visible, the meatball menu is visible"
  );
  let originalOuterWidth = win.outerWidth;
  await promiseStableResize(500, win);
  Assert.ok(!BrowserTestUtils.is_visible(addonButton));
  Assert.ok(!BrowserTestUtils.is_visible(starButton));
  Assert.ok(BrowserTestUtils.is_visible(meatballButton));

  info(
    "Remove the extension, check the only page action button is visible, the meatball menu is not visible"
  );
  let promiseUninstalled = promiseAddonUninstalled(extension.id);
  await extension.unload();
  await promiseUninstalled;
  Assert.ok(BrowserTestUtils.is_visible(starButton));
  Assert.ok(!BrowserTestUtils.is_visible(meatballButton));
  Assert.deepEqual(
    win.BrowserPageActions.urlbarButtonNodeForActionID(actionId),
    null
  );

  await promiseStableResize(originalOuterWidth, win);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function bookmark() {
  // We use an extension that shows a page action. We must add this additional
  // action because otherwise the meatball menu would not appear as an overflow
  // for a single action.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test contextMenu",
      page_action: { show_matches: ["<all_urls>"] },
    },

    useAddonManager: "temporary",
  });

  await extension.startup();
  const url = "data:text/html,<h1>A Page</h1>";
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(win);
  BrowserTestUtils.loadURI(win.gBrowser, url);
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);

  // The pageAction implementation enables the button at the next animation
  // frame, so before we look for the button we should wait one animation frame
  // as well.
  await promiseAnimationFrame(win);

  info("Shrink the window if necessary, check the meatball menu is visible");
  let originalOuterWidth = win.outerWidth;
  await promiseStableResize(500, win);

  let meatballButton = win.document.getElementById("pageActionButton");
  Assert.ok(BrowserTestUtils.is_visible(meatballButton));

  // Open the panel.
  await promisePageActionPanelOpen(win);

  // The bookmark button should read "Bookmark Current Tab" and not be starred.
  let bookmarkButton = win.document.getElementById("pageAction-panel-bookmark");
  Assert.equal(bookmarkButton.label, "Bookmark Current Tab");
  Assert.ok(!bookmarkButton.hasAttribute("starred"));

  // Click the button.
  let hiddenPromise = promisePageActionPanelHidden(win);
  let showPromise = BrowserTestUtils.waitForPopupEvent(
    win.StarUI.panel,
    "shown"
  );
  EventUtils.synthesizeMouseAtCenter(bookmarkButton, {}, win);
  await hiddenPromise;
  await showPromise;
  win.StarUI.panel.hidePopup();

  // Open the panel again.
  await promisePageActionPanelOpen(win);

  // The bookmark button should now read "Edit This Bookmark" and be starred.
  Assert.equal(bookmarkButton.label, "Edit This Bookmark");
  Assert.ok(bookmarkButton.hasAttribute("starred"));
  Assert.equal(bookmarkButton.getAttribute("starred"), "true");

  // Click it again.
  hiddenPromise = promisePageActionPanelHidden(win);
  showPromise = BrowserTestUtils.waitForPopupEvent(win.StarUI.panel, "shown");
  EventUtils.synthesizeMouseAtCenter(bookmarkButton, {}, win);
  await hiddenPromise;
  await showPromise;

  let onItemRemovedPromise = PlacesTestUtils.waitForNotification(
    "bookmark-removed",
    events => events.some(event => event.url == url),
    "places"
  );

  // Click the remove-bookmark button in the panel.
  win.StarUI._element("editBookmarkPanelRemoveButton").click();

  // Wait for the bookmark to be removed before continuing.
  await onItemRemovedPromise;

  // Open the panel again.
  await promisePageActionPanelOpen(win);

  // The bookmark button should read "Bookmark Current Tab" and not be starred.
  Assert.equal(bookmarkButton.label, "Bookmark Current Tab");
  Assert.ok(!bookmarkButton.hasAttribute("starred"));

  // Done.
  hiddenPromise = promisePageActionPanelHidden();
  win.BrowserPageActions.panelNode.hidePopup();
  await hiddenPromise;

  info("Remove the extension");
  let promiseUninstalled = promiseAddonUninstalled(extension.id);
  await extension.unload();
  await promiseUninstalled;

  await promiseStableResize(originalOuterWidth, win);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_disabledPageAction_hidden_in_protonOverflowMenu() {
  // Make sure that proton is enabled for this test (indipendently from its
  // default value set on the current build).
  await SpecialPowers.pushPrefEnv({
    set: [["browser.proton.enabled", true]],
  });
  // Make sure the overflow menu urlbar button is visible (indipendently from
  // the current size of the Firefox window).
  BrowserPageActions.mainButtonNode.style.visibility = "visible";
  registerCleanupFunction(() => {
    BrowserPageActions.mainButtonNode.style.removeProperty("visibility");
  });

  const extension = ExtensionTestUtils.loadExtension({
    manifest: { page_action: {} },
    async background() {
      const { browser } = this;
      const [tab] = await browser.tabs.query({
        active: true,
        currentWindow: true,
      });
      browser.test.assertTrue(tab, "Got an active tab as expected");
      browser.test.onMessage.addListener(async msg => {
        switch (msg) {
          case "show-pageAction":
            await browser.pageAction.show(tab.id);
            break;
          case "hide-pageAction":
            await browser.pageAction.hide(tab.id);
            break;
          default:
            browser.test.fail(`Unexpected message received: ${msg}`);
        }
        browser.test.sendMessage(`${msg}:done`);
      });
    },
  });

  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    const win = browser.ownerGlobal;
    const promisePageActionPanelClosed = async () => {
      let popupHiddenPromise = promisePageActionPanelHidden(win);
      win.BrowserPageActions.panelNode.hidePopup();
      await popupHiddenPromise;
    };

    await extension.startup();
    const widgetId = ExtensionCommon.makeWidgetId(extension.id);

    info(
      "Show pageAction and verify it is visible in the urlbar overflow menu"
    );
    extension.sendMessage("show-pageAction");
    await extension.awaitMessage("show-pageAction:done");
    await promisePageActionPanelOpen(win);
    let pageActionNode = win.BrowserPageActions.panelButtonNodeForActionID(
      widgetId
    );
    ok(
      pageActionNode && BrowserTestUtils.is_visible(pageActionNode),
      "enabled pageAction should be visible in the urlbar overflow menu"
    );

    info("Hide pageAction and verify it is hidden in the urlbar overflow menu");
    extension.sendMessage("hide-pageAction");
    await extension.awaitMessage("hide-pageAction:done");

    await BrowserTestUtils.waitForCondition(
      () => !win.BrowserPageActions.panelButtonNodeForActionID(widgetId),
      "Wait for the disabled pageAction to be removed from the urlbar overflow menu"
    );

    await promisePageActionPanelClosed();

    info("Reopen the urlbar overflow menu");
    await promisePageActionPanelOpen(win);
    ok(
      !win.BrowserPageActions.panelButtonNodeForActionID(widgetId),
      "Disabled pageAction is still removed as expected"
    );

    await promisePageActionPanelClosed();
    await extension.unload();
  });

  await SpecialPowers.popPrefEnv();
});
