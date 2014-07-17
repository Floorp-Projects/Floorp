/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Panel } = require("dev/panel");
const { Tool } = require("dev/toolbox");
const { Class } = require("sdk/core/heritage");


const LadybugPanel = Class({
  extends: Panel,
  label: "Ladybug",
  tooltip: "Debug client example",
  icon: "./plugin.png",
  url: "./index.html",
  setup: function({debuggee}) {
    this.debuggee = debuggee;
  },
  dispose: function() {
    delete this.debuggee;
  },
  onReady: function() {
    this.debuggee.start();
    this.postMessage("RDP", [this.debuggee]);
  },
});
exports.LadybugPanel = LadybugPanel;


const ladybug = new Tool({
  panels: { ladybug: LadybugPanel }
});
