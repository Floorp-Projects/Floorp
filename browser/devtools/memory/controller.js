/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
const { loader, require } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

const { Task } = require("resource://gre/modules/Task.jsm");
const { Heritage, ViewHelpers, WidgetMethods } = require("resource:///modules/devtools/ViewHelpers.jsm");

/**
 * The current target, toolbox and MemoryFront, set by this tool's host.
 */
var gToolbox, gTarget, gFront;

/**
 * Initializes the profiler controller and views.
 */
const MemoryController = {
  initialize: Task.async(function *() {

  }),

  destroy: Task.async(function *() {

  })
};
