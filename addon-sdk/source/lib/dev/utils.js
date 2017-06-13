/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const { DevToolsShim } = Cu.import("chrome://devtools-shim/content/DevToolsShim.jsm", {});

const { getActiveTab } = require("../sdk/tabs/utils");
const { getMostRecentBrowserWindow } = require("../sdk/window/utils");

const targetFor = target => {
  target = target || getActiveTab(getMostRecentBrowserWindow());
  return DevToolsShim.getTargetForTab(target);
};

const getId = id => ((id.prototype && id.prototype.id) || id.id || id);

const getCurrentPanel = toolbox => toolbox.getCurrentPanel();
exports.getCurrentPanel = getCurrentPanel;

const openToolbox = (id, tab) => {
  id = getId(id);
  return DevToolsShim.showToolbox(targetFor(tab), id);
};
exports.openToolbox = openToolbox;

const closeToolbox = tab => DevToolsShim.closeToolbox(targetFor(tab));
exports.closeToolbox = closeToolbox;

const getToolbox = tab => DevToolsShim.getToolbox(targetFor(tab));
exports.getToolbox = getToolbox;

const openToolboxPanel = (id, tab) => {
  id = getId(id);
  return DevToolsShim.showToolbox(targetFor(tab), id).then(getCurrentPanel);
};
exports.openToolboxPanel = openToolboxPanel;
