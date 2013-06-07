/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Ci } = require("chrome");
const { observe } = require("../event/chrome");
const { open } = require("../event/dom");
const { windows } = require("../window/utils");
const { filter, merge, map, expand } = require("../event/utils");

// Function registers single shot event listeners for relevant window events
// that forward events to exported event stream.
function eventsFor(window) {
  let interactive = open(window, "DOMContentLoaded", { capture: true });
  let complete = open(window, "load", { capture: true });
  let states = merge([interactive, complete]);
  let changes = filter(states, function({target}) target === window.document);
  return map(changes, function({type, target}) {
    return { type: type, target: target.defaultView }
  });
}

// In addition to observing windows that are open we also observe windows
// that are already already opened in case they're in process of loading.
let opened = windows(null, { includePrivate: true });
let currentEvents = merge(opened.map(eventsFor));

// Register system event listeners for top level window open / close.
function rename({type, target, data}) {
  return { type: rename[type], target: target, data: data }
}
rename.domwindowopened = "open";
rename.domwindowclosed = "close";

let openEvents = map(observe("domwindowopened"), rename);
let closeEvents = map(observe("domwindowclosed"), rename);
let futureEvents = expand(openEvents, function({target}) eventsFor(target));

let channel = merge([currentEvents, futureEvents,
                     openEvents, closeEvents]);
exports.events = channel;
