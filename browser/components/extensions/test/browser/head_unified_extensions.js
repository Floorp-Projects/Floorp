/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported closeExtensionsPanel,
            createExtensions,
            getUnifiedExtensionsItem,
            openExtensionsPanel,
            promiseDisableUnifiedExtensions,
            promiseEnableUnifiedExtensions,
            promiseUnifiedExtensionsInitialized,
            promiseUnifiedExtensionsWidgetExist
*/

const promiseUnifiedExtensionsWidgetExists = async win => {
  await BrowserTestUtils.waitForCondition(
    () => !!win.CustomizableUI.getWidget("unified-extensions"),
    "Wait unified-extensions widget"
  );
};

const promiseUnifiedExtensionsInitialized = async win => {
  await new Promise(resolve => {
    win.requestIdleCallback(resolve);
  });
  await TestUtils.waitForCondition(
    () => win.gUnifiedExtensions._initialized,
    "Wait gUnifiedExtensions to have been initialized"
  );
};

const promiseEnableUnifiedExtensions = async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.unifiedExtensions.enabled", true]],
  });

  const win = await BrowserTestUtils.openNewBrowserWindow();
  await promiseUnifiedExtensionsInitialized(win);
  await promiseUnifiedExtensionsWidgetExists(win);
  return win;
};

const promiseDisableUnifiedExtensions = async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.unifiedExtensions.enabled", false]],
  });

  const win = await BrowserTestUtils.openNewBrowserWindow();
  await promiseUnifiedExtensionsInitialized(win);
  await promiseUnifiedExtensionsWidgetExists(win);
  return win;
};

const getListView = win => {
  return PanelMultiView.getViewNode(win.document, "unified-extensions-view");
};

const openExtensionsPanel = async win => {
  const { button } = win.gUnifiedExtensions;
  ok(button, "expected button");

  const listView = getListView(win);
  ok(listView, "expected list view");

  const viewShown = BrowserTestUtils.waitForEvent(listView, "ViewShown");
  button.click();
  await viewShown;
};

const closeExtensionsPanel = async win => {
  const { button } = win.gUnifiedExtensions;
  ok(button, "expected button");

  const hidden = BrowserTestUtils.waitForEvent(
    win.document,
    "popuphidden",
    true
  );
  button.click();
  await hidden;
};

const getUnifiedExtensionsItem = (win, extensionId) => {
  return getListView(win).querySelector(
    `unified-extensions-item[extension-id="${extensionId}"]`
  );
};

let extensionsCreated = 0;
const createExtensions = (
  arrayOfManifestData,
  { useAddonManager = true } = {}
) => {
  return arrayOfManifestData.map(manifestData =>
    ExtensionTestUtils.loadExtension({
      manifest: {
        name: "default-extension-name",
        applications: {
          gecko: { id: `@ext-${extensionsCreated++}` },
        },
        ...manifestData,
      },
      useAddonManager: useAddonManager ? "temporary" : undefined,
    })
  );
};
