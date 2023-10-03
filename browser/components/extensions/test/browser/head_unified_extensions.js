/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported clickUnifiedExtensionsItem,
            closeExtensionsPanel,
            createExtensions,
            ensureMaximizedWindow,
            getMessageBars,
            getUnifiedExtensionsItem,
            openExtensionsPanel,
            openUnifiedExtensionsContextMenu,
            promiseSetToolbarVisibility
*/

const getListView = (win = window) => {
  const { panel } = win.gUnifiedExtensions;
  ok(panel, "expected panel to be created");
  return panel.querySelector("#unified-extensions-view");
};

const openExtensionsPanel = async (win = window) => {
  const { button } = win.gUnifiedExtensions;
  ok(button, "expected button");

  const listView = getListView(win);
  ok(listView, "expected list view");

  const viewShown = BrowserTestUtils.waitForEvent(listView, "ViewShown");
  button.click();
  await viewShown;
};

const closeExtensionsPanel = async (win = window) => {
  const { button } = win.gUnifiedExtensions;
  ok(button, "expected button");

  const hidden = BrowserTestUtils.waitForEvent(
    win.gUnifiedExtensions.panel,
    "popuphidden",
    true
  );
  button.click();
  await hidden;
};

const getUnifiedExtensionsItem = (extensionId, win = window) => {
  const view = getListView(win);

  // First try to find a CUI widget, otherwise a custom element when the
  // extension does not have a browser action.
  return (
    view.querySelector(`toolbaritem[data-extensionid="${extensionId}"]`) ||
    view.querySelector(`unified-extensions-item[extension-id="${extensionId}"]`)
  );
};

const openUnifiedExtensionsContextMenu = async (extensionId, win = window) => {
  const item = getUnifiedExtensionsItem(extensionId, win);
  ok(item, `expected item for extensionId=${extensionId}`);
  const button = item.querySelector(".unified-extensions-item-menu-button");
  ok(button, "expected menu button");
  // Make sure the button is visible before clicking on it (below) since the
  // list of extensions can have a scrollbar (when there are many extensions
  // and/or the window is small-ish).
  button.scrollIntoView({ block: "center" });

  const menu = win.document.getElementById("unified-extensions-context-menu");
  ok(menu, "expected menu");

  const shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  // Use primary button click to open the context menu.
  EventUtils.synthesizeMouseAtCenter(button, {}, win);
  await shown;

  return menu;
};

const clickUnifiedExtensionsItem = async (
  win,
  extensionId,
  forceEnableButton = false
) => {
  // The panel should be closed automatically when we click an extension item.
  await openExtensionsPanel(win);

  const item = getUnifiedExtensionsItem(extensionId, win);
  ok(item, `expected item for ${extensionId}`);

  // The action button should be disabled when users aren't supposed to click
  // on it but it might still be useful to re-enable it for testing purposes.
  if (forceEnableButton) {
    let actionButton = item.querySelector(
      ".unified-extensions-item-action-button"
    );
    actionButton.disabled = false;
    ok(!actionButton.disabled, "action button was force-enabled");
  }

  // Similar to `openUnifiedExtensionsContextMenu()`, we make sure the item is
  // visible before clicking on it to prevent intermittents.
  item.scrollIntoView({ block: "center" });

  const popupHidden = BrowserTestUtils.waitForEvent(
    win.document,
    "popuphidden",
    true
  );
  EventUtils.synthesizeMouseAtCenter(item, {}, win);
  await popupHidden;
};

const createExtensions = (
  arrayOfManifestData,
  { useAddonManager = true, incognitoOverride, files } = {}
) => {
  return arrayOfManifestData.map(manifestData =>
    ExtensionTestUtils.loadExtension({
      manifest: {
        name: "default-extension-name",
        ...manifestData,
      },
      useAddonManager: useAddonManager ? "temporary" : undefined,
      incognitoOverride,
      files,
    })
  );
};

/**
 * Given a window, this test helper resizes it so that the window takes most of
 * the available screen size (unless the window is already maximized).
 */
const ensureMaximizedWindow = async win => {
  info("ensuring maximized window...");

  // Make sure we wait for window position to have settled
  // to avoid unexpected failures.
  let samePositionTimes = 0;
  let lastScreenTop = win.screen.top;
  let lastScreenLeft = win.screen.left;
  win.moveTo(0, 0);
  await TestUtils.waitForCondition(() => {
    let isSamePosition =
      lastScreenTop === win.screen.top && lastScreenLeft === win.screen.left;
    if (!isSamePosition) {
      lastScreenTop = win.screen.top;
      lastScreenLeft = win.screen.left;
    }
    samePositionTimes = isSamePosition ? samePositionTimes + 1 : 0;
    return samePositionTimes === 10;
  }, "Wait for the chrome window position to settle");

  const widthDiff = Math.max(win.screen.availWidth - win.outerWidth, 0);
  const heightDiff = Math.max(win.screen.availHeight - win.outerHeight, 0);

  if (widthDiff || heightDiff) {
    info(
      `resizing window... widthDiff=${widthDiff} - heightDiff=${heightDiff}`
    );
    win.windowUtils.ensureDirtyRootFrame();
    win.resizeBy(widthDiff, heightDiff);
  } else {
    info(`not resizing window!`);
  }

  // Make sure we wait for window size to have settled.
  let lastOuterWidth = win.outerWidth;
  let lastOuterHeight = win.outerHeight;
  let sameSizeTimes = 0;
  await TestUtils.waitForCondition(() => {
    const isSameSize =
      win.outerWidth === lastOuterWidth && win.outerHeight === lastOuterHeight;
    if (!isSameSize) {
      lastOuterWidth = win.outerWidth;
      lastOuterHeight = win.outerHeight;
    }
    sameSizeTimes = isSameSize ? sameSizeTimes + 1 : 0;
    return sameSizeTimes === 10;
  }, "Wait for the chrome window size to settle");
};

const promiseSetToolbarVisibility = (toolbar, visible) => {
  const visibilityChanged = BrowserTestUtils.waitForMutationCondition(
    toolbar,
    { attributeFilter: ["collapsed"] },
    () => toolbar.collapsed != visible
  );
  setToolbarVisibility(toolbar, visible, undefined, false);
  return visibilityChanged;
};

const getMessageBars = (win = window) => {
  const { panel } = win.gUnifiedExtensions;
  return panel.querySelectorAll(
    "#unified-extensions-messages-container > moz-message-bar"
  );
};
