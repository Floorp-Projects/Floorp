/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* globals dump */

const { gDevTools } = require("devtools/client/framework/devtools");
const { TargetFactory } = require("devtools/client/framework/target");
const { addTab, removeTab } = require("devtools/client/performance/test/helpers/tab-utils");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

/**
 * Initializes a toolbox panel in a new tab.
 */
exports.initPanelInNewTab = function* ({ tool, url, win }, options = {}) {
  let tab = yield addTab({ url, win }, options);
  return (yield exports.initPanelInTab({ tool, tab }));
};

/**
 * Initializes a toolbox panel in the specified tab.
 */
exports.initPanelInTab = function* ({ tool, tab }) {
  dump(`Initializing a ${tool} panel.\n`);

  let target = TargetFactory.forTab(tab);
  yield target.makeRemote();

  // Open a toolbox and wait for the connection to the performance actors
  // to be opened. This is necessary because of the WebConsole's
  // `profile` and `profileEnd` methods.
  let toolbox = yield gDevTools.showToolbox(target, tool);
  yield toolbox.initPerformance();

  let panel = toolbox.getCurrentPanel();
  return { target, toolbox, panel };
};

/**
 * Initializes a performance panel in a new tab.
 */
exports.initPerformanceInNewTab = function* ({ url, win }, options = {}) {
  let tab = yield addTab({ url, win }, options);
  return (yield exports.initPerformanceInTab({ tab }));
};

/**
 * Initializes a performance panel in the specified tab.
 */
exports.initPerformanceInTab = function* ({ tab }) {
  return (yield exports.initPanelInTab({
    tool: "performance",
    tab: tab
  }));
};

/**
 * Initializes a webconsole panel in a new tab.
 * Returns a console property that allows calls to `profile` and `profileEnd`.
 */
exports.initConsoleInNewTab = function* ({ url, win }, options = {}) {
  let tab = yield addTab({ url, win }, options);
  return (yield exports.initConsoleInTab({ tab }));
};

/**
 * Initializes a webconsole panel in the specified tab.
 * Returns a console property that allows calls to `profile` and `profileEnd`.
 */
exports.initConsoleInTab = function* ({ tab }) {
  let { target, toolbox, panel } = yield exports.initPanelInTab({
    tool: "webconsole",
    tab: tab
  });

  let consoleMethod = function* (method, label, event) {
    let recordingEventReceived = once(toolbox.performance, event);
    if (label === undefined) {
      yield panel.hud.jsterm.execute(`console.${method}()`);
    } else {
      yield panel.hud.jsterm.execute(`console.${method}("${label}")`);
    }
    yield recordingEventReceived;
  };

  let profile = function* (label) {
    return yield consoleMethod("profile", label, "recording-started");
  };

  let profileEnd = function* (label) {
    return yield consoleMethod("profileEnd", label, "recording-stopped");
  };

  return { target, toolbox, panel, console: { profile, profileEnd } };
};

/**
 * Tears down a toolbox panel and removes an associated tab.
 */
exports.teardownToolboxAndRemoveTab = function* (panel, options) {
  dump("Destroying panel.\n");

  let tab = panel.target.tab;
  yield panel.toolbox.destroy();
  yield removeTab(tab, options);
};
