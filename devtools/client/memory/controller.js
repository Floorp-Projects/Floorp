/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Task } = require("resource://gre/modules/Task.jsm");
const Store = require("./store");

/**
 * The current target, toolbox and MemoryFront, set by this tool's host.
 */
var gToolbox, gTarget, gFront;

const REDUX_METHODS_TO_PIPE = ["dispatch", "subscribe", "getState"];

const MemoryController = exports.MemoryController = function ({ toolbox, target, front }) {
  this.store = Store();
  this.toolbox = toolbox;
  this.target = target;
  this.front = front;
};

REDUX_METHODS_TO_PIPE.map(m =>
  MemoryController.prototype[m] = function (...args) { return this.store[m](...args); });

MemoryController.prototype.destroy = function () {
  this.store = this.toolbox = this.target = this.front = null;
};
