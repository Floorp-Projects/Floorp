/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported openToolboxForTab, closeToolboxForTab,
            registerBlankToolboxPanel, TOOLBOX_BLANK_PANEL_ID, assertDevToolsExtensionEnabled */

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
