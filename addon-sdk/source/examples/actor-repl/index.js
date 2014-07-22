/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Panel } = require("dev/panel");
const { Tool } = require("dev/toolbox");
const { Class } = require("sdk/core/heritage");


const REPLPanel = Class({
  extends: Panel,
  label: "Actor REPL",
  tooltip: "Firefox debugging protocol REPL",
  icon: "./robot.png",
  url: "./index.html",
  setup: function({debuggee}) {
    this.debuggee = debuggee;
  },
  dispose: function() {
    this.debuggee = null;
  },
  onReady: function() {
    console.log("repl panel document is interactive");
    this.debuggee.start();
    this.postMessage("RDP", [this.debuggee]);
  },
  onLoad: function() {
    console.log("repl panel document is fully loaded");
  }
});
exports.REPLPanel = REPLPanel;


const replTool = new Tool({
  panels: { repl: REPLPanel }
});
