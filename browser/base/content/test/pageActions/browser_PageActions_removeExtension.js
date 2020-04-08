"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.import(
  "resource://testing-common/EnterprisePolicyTesting.jsm"
);

const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const TELEMETRY_EVENTS_FILTERS = {
  category: "addonsManager",
  method: "action",
};

// Initialization. Must run first.
add_task(async function init() {
  // The page action urlbar button, and therefore the panel, is only shown when
  // the current tab is actionable -- i.e., a normal web page.  about:blank is
  // not, so open a new tab first thing, and close it when this test is done.
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "http://example.com/",
  });

  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function contextMenu_removeExtension_panel() {
  Services.telemetry.clearEvents();

  // We use an extension that shows a page action so that we can test the
  // "remove extension" item in the context menu.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test contextMenu",
      page_action: { show_matches: ["<all_urls>"] },
    },

    useAddonManager: "temporary",
  });

  await extension.startup();

  let actionId = ExtensionCommon.makeWidgetId(extension.id);

  // Open the panel and then open the context menu on the action's item.
  await promisePageActionPanelOpen();
  let panelButton = BrowserPageActions.panelButtonNodeForActionID(actionId);
  let contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(panelButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;

  let removeExtensionItem = getRemoveExtensionItem();
  Assert.ok(removeExtensionItem, "'Remove' item exists");
  Assert.ok(!removeExtensionItem.hidden, "'Remove' item is visible");
  Assert.ok(!removeExtensionItem.disabled, "'Remove' item is not disabled");

  // Click the "remove extension" item, a prompt should be displayed and then
  // the add-on should be uninstalled. We mock the prompt service to confirm
  // the removal of the add-on.
  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  let addonUninstalledPromise = promiseAddonUninstalled(extension.id);
  mockPromptService();
  EventUtils.synthesizeMouseAtCenter(removeExtensionItem, {});
  await Promise.all([contextMenuPromise, addonUninstalledPromise]);

  // Done, clean up.
  await extension.unload();

  TelemetryTestUtils.assertEvents(
    [
      {
        object: "pageAction",
        value: "accepted",
        extra: { addonId: extension.id, action: "uninstall" },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );

  // urlbar tests that run after this one can break if the mouse is left over
  // the area where the urlbar popup appears, which seems to happen due to the
  // above synthesized mouse events.  Move it over the urlbar.
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, { type: "mousemove" });
  gURLBar.focus();
});

add_task(async function contextMenu_removeExtension_urlbar() {
  Services.telemetry.clearEvents();

  // We use an extension that shows a page action so that we can test the
  // "remove extension" item in the context menu.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test contextMenu",
      page_action: { show_matches: ["<all_urls>"] },
    },

    useAddonManager: "temporary",
  });

  await extension.startup();
  // The pageAction implementation enables the button at the next animation
  // frame, so before we look for the button we should wait one animation frame
  // as well.
  await promiseAnimationFrame();

  let actionId = ExtensionCommon.makeWidgetId(extension.id);

  // Open the context menu on the action's urlbar button.
  let urlbarButton = BrowserPageActions.urlbarButtonNodeForActionID(actionId);
  let contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(urlbarButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;

  let removeExtensionItem = getRemoveExtensionItem();
  Assert.ok(removeExtensionItem, "'Remove' item exists");
  Assert.ok(!removeExtensionItem.hidden, "'Remove' item is visible");
  Assert.ok(!removeExtensionItem.disabled, "'Remove' item is not disabled");

  // Click the "remove extension" item, a prompt should be displayed and then
  // the add-on should be uninstalled. We mock the prompt service to cancel the
  // removal of the add-on.
  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  let promptService = mockPromptService();
  let promptCancelledPromise = new Promise(resolve => {
    promptService.confirmEx = () => resolve();
  });
  EventUtils.synthesizeMouseAtCenter(removeExtensionItem, {});
  await Promise.all([contextMenuPromise, promptCancelledPromise]);

  // Done, clean up.
  await extension.unload();

  TelemetryTestUtils.assertEvents(
    [
      {
        object: "pageAction",
        value: "cancelled",
        extra: { addonId: extension.id, action: "uninstall" },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );

  // urlbar tests that run after this one can break if the mouse is left over
  // the area where the urlbar popup appears, which seems to happen due to the
  // above synthesized mouse events.  Move it over the urlbar.
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, { type: "mousemove" });
  gURLBar.focus();
});

add_task(async function contextMenu_removeExtension_disabled_in_panel() {
  // We use an extension that shows a page action so that we can test the
  // "remove extension" item in the context menu.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test contextMenu",
      page_action: { show_matches: ["<all_urls>"] },
    },

    useAddonManager: "temporary",
  });

  await extension.startup();
  // Add a policy to prevent the add-on from being uninstalled.
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      Extensions: {
        Locked: [extension.id],
      },
    },
  });

  let actionId = ExtensionCommon.makeWidgetId(extension.id);

  // Open the panel and then open the context menu on the action's item.
  await promisePageActionPanelOpen();
  let panelButton = BrowserPageActions.panelButtonNodeForActionID(actionId);
  let contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(panelButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;

  let removeExtensionItem = getRemoveExtensionItem();
  Assert.ok(removeExtensionItem, "'Remove' item exists");
  Assert.ok(!removeExtensionItem.hidden, "'Remove' item is visible");
  Assert.ok(removeExtensionItem.disabled, "'Remove' item is disabled");

  // Press escape to hide the context menu.
  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  EventUtils.synthesizeKey("KEY_Escape");
  await contextMenuPromise;

  // Done, clean up.
  await extension.unload();
  await EnterprisePolicyTesting.setupPolicyEngineWithJson("");

  // urlbar tests that run after this one can break if the mouse is left over
  // the area where the urlbar popup appears, which seems to happen due to the
  // above synthesized mouse events.  Move it over the urlbar.
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, { type: "mousemove" });
  gURLBar.focus();
});

add_task(async function contextMenu_removeExtension_disabled_in_urlbar() {
  // We use an extension that shows a page action so that we can test the
  // "remove extension" item in the context menu.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test contextMenu",
      page_action: { show_matches: ["<all_urls>"] },
    },

    useAddonManager: "temporary",
  });

  await extension.startup();
  // The pageAction implementation enables the button at the next animation
  // frame, so before we look for the button we should wait one animation frame
  // as well.
  await promiseAnimationFrame();
  // Add a policy to prevent the add-on from being uninstalled.
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      Extensions: {
        Locked: [extension.id],
      },
    },
  });

  let actionId = ExtensionCommon.makeWidgetId(extension.id);

  // Open the context menu on the action's urlbar button.
  let urlbarButton = BrowserPageActions.urlbarButtonNodeForActionID(actionId);
  let contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(urlbarButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;

  let removeExtensionItem = getRemoveExtensionItem();
  Assert.ok(removeExtensionItem, "'Remove' item exists");
  Assert.ok(!removeExtensionItem.hidden, "'Remove' item is visible");
  Assert.ok(removeExtensionItem.disabled, "'Remove' item is disabled");

  // Press escape to hide the context menu.
  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  EventUtils.synthesizeKey("KEY_Escape");
  await contextMenuPromise;

  // Done, clean up.
  await extension.unload();
  await EnterprisePolicyTesting.setupPolicyEngineWithJson("");

  // urlbar tests that run after this one can break if the mouse is left over
  // the area where the urlbar popup appears, which seems to happen due to the
  // above synthesized mouse events.  Move it over the urlbar.
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, { type: "mousemove" });
  gURLBar.focus();
});

function promiseAddonUninstalled(addonId) {
  return new Promise(resolve => {
    let listener = {};
    listener.onUninstalled = addon => {
      if (addon.id == addonId) {
        AddonManager.removeAddonListener(listener);
        resolve();
      }
    };
    AddonManager.addAddonListener(listener);
  });
}

function mockPromptService() {
  let { prompt } = Services;

  let promptService = {
    // The prompt returns 1 for cancelled and 0 for accepted.
    _response: 0,
    QueryInterface: ChromeUtils.generateQI([Ci.nsIPromptService]),
    confirmEx: () => promptService._response,
  };

  Services.prompt = promptService;

  registerCleanupFunction(() => {
    Services.prompt = prompt;
  });

  return promptService;
}

function getRemoveExtensionItem() {
  return document.querySelector(
    "#pageActionContextMenu > menuitem[label='Remove Extension']"
  );
}

async function promiseAnimationFrame(win = window) {
  await new Promise(resolve => win.requestAnimationFrame(resolve));

  let { tm } = Services;
  return new Promise(resolve => tm.dispatchToMainThread(resolve));
}
