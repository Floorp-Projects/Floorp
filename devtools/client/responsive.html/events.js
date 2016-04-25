/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { filter, map, merge } = require("sdk/event/utils");
const { events: browserEvents } = require("sdk/browser/events");
const { events: tabEvents } = require("sdk/tab/events");
const { getTabs, getActiveTab, getOwnerWindow } = require("sdk/tabs/utils");

const tabSelect = filter(tabEvents, e => e.type === "TabSelect");
const tabClose = filter(tabEvents, e => e.type === "TabClose");
const windowOpen = filter(browserEvents, e => e.type === "load");
const windowClose = filter(browserEvents, e => e.type === "close");

// The `activate` event stream, observes when any tab is activated.
// It has the activated `tab` as argument.
const activate = merge([
  map(tabSelect, ({target}) => target),
  map(windowOpen, ({target}) => getActiveTab(target))
]);
exports.activate = activate;

// The `close` event stream, observes when any tab or any window is closed.
// The event has an object as argument, with the `tabs` involved in the closing
// process and their owner window.
const close = merge([
  map(tabClose, ({target}) => ({
    window: getOwnerWindow(target),
    tabs: [target]
  })),
  map(windowClose, ({target}) => ({
    window: target,
    tabs: getTabs(target)
  }))
]);
exports.close = close;
