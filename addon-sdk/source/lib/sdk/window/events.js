/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Ci, Cu } = require("chrome");
const { observe } = require("../event/chrome");
const { open } = require("../event/dom");
const { windows } = require("../window/utils");
const { filter, merge, map, expand } = require("../event/utils");

function documentMatches(weakWindow, event) {
  let window = weakWindow.get();
  return window && event.target === window.document;
}

function makeStrictDocumentFilter(window) {
  // Note: Do not define a closure within this function.  Otherwise
  //       you may leak the window argument.
  let weak = Cu.getWeakReference(window);
  return documentMatches.bind(null, weak);
}

function toEventWithDefaultViewTarget({type, target}) {
  return { type: type, target: target.defaultView }
}

// Function registers single shot event listeners for relevant window events
// that forward events to exported event stream.
function eventsFor(window) {
  // NOTE: Do no use pass a closure from this function into a stream
  //       transform function.  You will capture the window in the
  //       closure and leak the window until the event stream is
  //       completely closed.
  let interactive = open(window, "DOMContentLoaded", { capture: true });
  let complete = open(window, "load", { capture: true });
  let states = merge([interactive, complete]);
  let changes = filter(states, makeStrictDocumentFilter(window));
  return map(changes, toEventWithDefaultViewTarget);
}

// Create our event channels.  We do this in a separate function to
// minimize the chance of leaking intermediate objects on the global.
function makeEvents() {
  // In addition to observing windows that are open we also observe windows
  // that are already already opened in case they're in process of loading.
  var opened = windows(null, { includePrivate: true });
  var currentEvents = merge(opened.map(eventsFor));

  // Register system event listeners for top level window open / close.
  function rename({type, target, data}) {
    return { type: rename[type], target: target, data: data }
  }
  rename.domwindowopened = "open";
  rename.domwindowclosed = "close";

  var openEvents = map(observe("domwindowopened"), rename);
  var closeEvents = map(observe("domwindowclosed"), rename);
  var futureEvents = expand(openEvents, ({target}) => eventsFor(target));

  return merge([currentEvents, futureEvents, openEvents, closeEvents]);
}

exports.events = makeEvents();
