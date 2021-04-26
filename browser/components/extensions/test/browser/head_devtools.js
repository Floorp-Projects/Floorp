/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported
     assertDevToolsExtensionEnabled,
     closeToolboxForTab,
     navigateToWithDevToolsOpen
     openToolboxForTab,
     registerBlankToolboxPanel,
     TOOLBOX_BLANK_PANEL_ID,
*/

ChromeUtils.defineModuleGetter(
  this,
  "loader",
  "resource://devtools/shared/Loader.jsm"
);
XPCOMUtils.defineLazyGetter(this, "gDevTools", () => {
  const { gDevTools } = loader.require("devtools/client/framework/devtools");
  return gDevTools;
});

ChromeUtils.defineModuleGetter(
  this,
  "DevToolsShim",
  "chrome://devtools-startup/content/DevToolsShim.jsm"
);

const TOOLBOX_BLANK_PANEL_ID = "testBlankPanel";

// Register a blank custom tool so that we don't need to wait the webconsole
// to be fully loaded/unloaded to prevent intermittent failures (related
// to a webconsole that is still loading when the test has been completed).
async function registerBlankToolboxPanel() {
  const testBlankPanel = {
    id: TOOLBOX_BLANK_PANEL_ID,
    url: "about:blank",
    label: "Blank Tool",
    isTargetSupported() {
      return true;
    },
    build(iframeWindow, toolbox) {
      return Promise.resolve({
        target: toolbox.target,
        toolbox: toolbox,
        isReady: true,
        panelDoc: iframeWindow.document,
        destroy() {},
      });
    },
  };

  registerCleanupFunction(() => {
    gDevTools.unregisterTool(testBlankPanel.id);
  });

  gDevTools.registerTool(testBlankPanel);
}

async function openToolboxForTab(tab, panelId = TOOLBOX_BLANK_PANEL_ID) {
  if (
    panelId == TOOLBOX_BLANK_PANEL_ID &&
    !gDevTools.getToolDefinition(panelId)
  ) {
    info(`Registering ${TOOLBOX_BLANK_PANEL_ID} tool to the developer tools`);
    registerBlankToolboxPanel();
  }

  const toolbox = await gDevTools.showToolboxForTab(tab, { toolId: panelId });
  const { url, outerWindowID } = toolbox.target.form;
  info(
    `Developer toolbox opened on panel "${panelId}" for target ${JSON.stringify(
      { url, outerWindowID }
    )}`
  );
  return toolbox;
}

async function closeToolboxForTab(tab) {
  const toolbox = await gDevTools.getToolboxForTab(tab);
  const target = toolbox.target;
  const { url, outerWindowID } = target.form;

  await gDevTools.closeToolboxForTab(tab);
  await target.destroy();

  info(
    `Developer toolbox closed for target ${JSON.stringify({
      url,
      outerWindowID,
    })}`
  );
}

function assertDevToolsExtensionEnabled(uuid, enabled) {
  for (let toolbox of DevToolsShim.getToolboxes()) {
    is(
      enabled,
      !!toolbox.isWebExtensionEnabled(uuid),
      `extension is ${enabled ? "enabled" : "disabled"} on toolbox`
    );
  }
}

/**
 * Navigate the currently selected tab to a new URL and wait for it to load.
 * Also wait for the toolbox to attach to the new target, if we navigated
 * to a new process.
 *
 * @param {Object} tab The tab to redirect.
 * @param {string} uri The url to be loaded in the current tab.
 * @param {boolean} isErrorPage You may pass `true` is the URL is an error
 *                    page. Otherwise BrowserTestUtils.browserLoaded will wait
 *                    for 'load' event, which never fires for error pages.
 *
 * @returns {Promise} A promise that resolves when the page has fully loaded.
 */
async function navigateToWithDevToolsOpen(tab, uri, isErrorPage = false) {
  const toolbox = await gDevTools.getToolboxForTab(tab);
  const target = toolbox.target;

  // If we're switching origins, we need to wait for the 'switched-target'
  // event to make sure everything is ready.
  // Navigating from/to pages loaded in the parent process, like about:robots,
  // also spawn new targets.
  // (If target switching is disabled, the toolbox will reboot)
  const onTargetSwitched = toolbox.commands.targetCommand.once(
    "switched-target"
  );
  // Otherwise, if we don't switch target, it is safe to wait for navigate event.
  const onNavigate = target.once("navigate");

  // If the current top-level target follows the window global lifecycle, a
  // target switch will occur regardless of process changes.
  const targetFollowsWindowLifecycle =
    target.targetForm.followWindowGlobalLifeCycle;

  info(`Load document "${uri}"`);
  const browser = gBrowser.selectedBrowser;
  const currentPID = browser.browsingContext.currentWindowGlobal.osPid;
  const currentBrowsingContextID = browser.browsingContext.id;
  const onBrowserLoaded = BrowserTestUtils.browserLoaded(
    browser,
    false,
    null,
    isErrorPage
  );
  BrowserTestUtils.loadURI(browser, uri);

  info(`Waiting for page to be loaded…`);
  await onBrowserLoaded;
  info(`→ page loaded`);

  // Compare the PIDs (and not the toolbox's targets) as PIDs are updated also immediately,
  // while target may be updated slightly later.
  const switchedToAnotherProcess =
    currentPID !== browser.browsingContext.currentWindowGlobal.osPid;
  const switchedToAnotherBrowsingContext =
    currentBrowsingContextID !== browser.browsingContext.id;

  // If:
  // - the tab navigated to another process, or,
  // - the tab navigated to another browsing context, or,
  // - if the old target follows the window lifecycle
  // then, expect a target switching.
  if (
    switchedToAnotherProcess ||
    targetFollowsWindowLifecycle ||
    switchedToAnotherBrowsingContext
  ) {
    info(`Waiting for target switch…`);
    await onTargetSwitched;
    info(`→ switched-target emitted`);
  } else {
    info(`Waiting for target 'navigate' event…`);
    await onNavigate;
    info(`→ 'navigate' emitted`);
  }
}
