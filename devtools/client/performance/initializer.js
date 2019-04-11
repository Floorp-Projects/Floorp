/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BrowserLoader } = ChromeUtils.import("resource://devtools/client/shared/browser-loader.js");
const { require } = BrowserLoader({
  baseURI: "resource://devtools/client/performance/",
  window: window,
});

const { PerformanceController } = require("./performance-controller");
const { PerformanceView } = require("./performance-view");
const { DetailsView } = require("./views/details");
const { DetailsSubview } = require("./views/details-abstract-subview");
const { JsCallTreeView } = require("./views/details-js-call-tree");
const { JsFlameGraphView } = require("./views/details-js-flamegraph");
const { MemoryCallTreeView } = require("./views/details-memory-call-tree");
const { MemoryFlameGraphView } = require("./views/details-memory-flamegraph");
const { OverviewView } = require("./views/overview");
const { RecordingsView } = require("./views/recordings");
const { ToolbarView } = require("./views/toolbar");
const { WaterfallView } = require("./views/details-waterfall");

const EVENTS = require("./events");

/**
 * The performance panel used to only share modules through references on the window
 * object. We started cleaning this up and to require() explicitly in Bug 1524982, but
 * some modules and tests are still relying on those references so we keep exposing them
 * for the moment. Bug 1528777.
 */
window.PerformanceController = PerformanceController;
window.PerformanceView = PerformanceView;
window.DetailsView = DetailsView;
window.DetailsSubview = DetailsSubview;
window.JsCallTreeView = JsCallTreeView;
window.JsFlameGraphView = JsFlameGraphView;
window.MemoryCallTreeView = MemoryCallTreeView;
window.MemoryFlameGraphView = MemoryFlameGraphView;
window.OverviewView = OverviewView;
window.RecordingsView = RecordingsView;
window.ToolbarView = ToolbarView;
window.WaterfallView = WaterfallView;

window.EVENTS = EVENTS;

/**
 * DOM query helpers.
 */
/* exported $, $$ */
function $(selector, target = document) {
  return target.querySelector(selector);
}
window.$ = $;
function $$(selector, target = document) {
  return target.querySelectorAll(selector);
}
window.$$ = $$;
