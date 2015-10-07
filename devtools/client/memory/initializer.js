/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
const { require } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const { Task } = require("resource://gre/modules/Task.jsm");
const { MemoryController } = require("devtools/client/memory/controller");

/**
 * The current target, toolbox and MemoryFront, set by this tool's host.
 */
let gToolbox, gTarget, gFront;

/**
 * Initializes the profiler controller and views.
 */
var controller = null;
function initialize () {
  return Task.spawn(function *() {
    controller = new MemoryController({ toolbox: gToolbox, target: gTarget, front: gFront });
  });
}

function destroy () {
  return Task.spawn(function *() {
    controller.destroy();
  });
}
