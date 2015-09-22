/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const { gDevTools } = Cu.import("resource:///modules/devtools/client/framework/gDevTools.jsm", {});
const { devtools } = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});

const { getActiveTab } = require("../sdk/tabs/utils");
const { getMostRecentBrowserWindow } = require("../sdk/window/utils");

const targetFor = target => {
  target = target || getActiveTab(getMostRecentBrowserWindow());
  return devtools.TargetFactory.forTab(target);
};

const getId = id => ((id.prototype && id.prototype.id) || id.id || id);

const getCurrentPanel = toolbox => toolbox.getCurrentPanel();
exports.getCurrentPanel = getCurrentPanel;

const openToolbox = (id, tab) => {
  id = getId(id);
  return gDevTools.showToolbox(targetFor(tab), id);
};
exports.openToolbox = openToolbox;

const closeToolbox = tab => gDevTools.closeToolbox(targetFor(tab));
exports.closeToolbox = closeToolbox;

const getToolbox = tab => gDevTools.getToolbox(targetFor(tab));
exports.getToolbox = getToolbox;

const openToolboxPanel = (id, tab) => {
  id = getId(id);
  return gDevTools.showToolbox(targetFor(tab), id).then(getCurrentPanel);
};
exports.openToolboxPanel = openToolboxPanel;
