/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* globals dump */

const { gDevTools } = require("devtools/client/framework/devtools");
const {
  addTab,
  removeTab,
} = require("devtools/client/performance/test/helpers/tab-utils");
const {
  once,
} = require("devtools/client/performance/test/helpers/event-utils");

/**
 * Initializes a toolbox panel in a new tab.
 */
exports.initPanelInNewTab = async function({ tool, url, win }, options = {}) {
  const tab = await addTab({ url, win }, options);
  return exports.initPanelInTab({ tool, tab });
};

/**
 * Initializes a toolbox panel in the specified tab.
 */
exports.initPanelInTab = async function({ tool, tab }) {
  dump(`Initializing a ${tool} panel.\n`);

  // Open a toolbox and wait for the connection to the performance actors
  // to be opened. This is necessary because of the WebConsole's
  // `profile` and `profileEnd` methods.
  const toolbox = await gDevTools.showToolboxForTab(tab, { toolId: tool });
  const target = toolbox.target;
  // ensure that the performance front is ready
  await target.getFront("performance");

  const panel = toolbox.getCurrentPanel();
  return { target, toolbox, panel };
};

/**
 * Initializes a performance panel in a new tab.
 */
exports.initPerformanceInNewTab = async function({ url, win }, options = {}) {
  const tab = await addTab({ url, win }, options);
  return exports.initPerformanceInTab({ tab });
};

/**
 * Initializes a performance panel in the specified tab.
 */
exports.initPerformanceInTab = async function({ tab }) {
  return exports.initPanelInTab({
    tool: "performance",
    tab: tab,
  });
};

/**
 * Initializes a webconsole panel in a new tab.
 * Returns a console property that allows calls to `profile` and `profileEnd`.
 */
exports.initConsoleInNewTab = async function({ url, win }, options = {}) {
  const tab = await addTab({ url, win }, options);
  return exports.initConsoleInTab({ tab });
};

/**
 * Initializes a webconsole panel in the specified tab.
 * Returns a console property that allows calls to `profile` and `profileEnd`.
 */
exports.initConsoleInTab = async function({ tab }) {
  const { target, toolbox, panel } = await exports.initPanelInTab({
    tool: "webconsole",
    tab: tab,
  });

  const consoleMethod = async function(method, label, event) {
    const performanceFront = await toolbox.target.getFront("performance");
    const recordingEventReceived = once(performanceFront, event);
    const expression = label
      ? `console.${method}("${label}")`
      : `console.${method}()`;
    await panel.hud.ui.wrapper.dispatchEvaluateExpression(expression);
    await recordingEventReceived;
  };

  const profile = async function(label) {
    return consoleMethod("profile", label, "recording-started");
  };

  const profileEnd = async function(label) {
    return consoleMethod("profileEnd", label, "recording-stopped");
  };

  return { target, toolbox, panel, console: { profile, profileEnd } };
};

/**
 * Tears down a toolbox panel and removes an associated tab.
 */
exports.teardownToolboxAndRemoveTab = async function(panel) {
  dump("Destroying panel.\n");

  const tab = panel.target.localTab;
  await panel.toolbox.destroy();
  await removeTab(tab);
};
