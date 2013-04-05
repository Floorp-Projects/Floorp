/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Ci } = require("chrome");
const events = require("../system/events");
const { on, off, emit } = require("../event/core");
const { windows } = require("../window/utils");

// Object represents event channel on which all top level window events
// will be dispatched, allowing users to react to those evens.
const channel = {};
exports.events = channel;

const types = {
  domwindowopened: "open",
  domwindowclosed: "close",
}

// Utility function to query observer notification subject to get DOM window.
function nsIDOMWindow($) $.QueryInterface(Ci.nsIDOMWindow);

// Utility function used as system event listener that is invoked every time
// top level window is open. This function does two things:
// 1. Registers event listeners to track when document becomes interactive and
//    when it's done loading. This will become obsolete once Bug 843910 is
//    fixed.
// 2. Forwards event to an exported event stream.
function onOpen(event) {
  observe(nsIDOMWindow(event.subject));
  dispatch(event);
}

// Function registers single shot event listeners for relevant window events
// that forward events to exported event stream.
function observe(window) {
  function listener(event) {
    if (event.target === window.document) {
      window.removeEventListener(event.type, listener, true);
      emit(channel, "data", { type: event.type, target: window });
    }
  }

  // Note: we do not remove listeners on unload since on add-on unload we
  // nuke add-on sandbox that should allow GC-ing listeners. This also has
  // positive effects on add-on / firefox unloads.
  window.addEventListener("DOMContentLoaded", listener, true);
  window.addEventListener("load", listener, true);
  // TODO: Also add focus event listener so that can be forwarded to event
  // stream. It can be part of Bug 854982.
}

// Utility function that takes system notification event and forwards it to a
// channel in restructured form.
function dispatch({ type: topic, subject }) {
  emit(channel, "data", {
    topic: topic,
    type: types[topic],
    target: nsIDOMWindow(subject)
  });
}

// In addition to observing windows that are open we also observe windows
// that are already already opened in case they're in process of loading.
let opened = windows(null, { includePrivate: true });
opened.forEach(observe);

// Register system event listeners to forward messages on exported event
// stream. Note that by default only weak refs are kept by system events
// module so they will be GC-ed once add-on unloads and no manual cleanup
// is required. Also note that listeners are intentionally not inlined since
// to avoid premature GC-ing. Currently refs are kept by module scope and there
// for they remain alive.
events.on("domwindowopened", onOpen);
events.on("domwindowclosed", dispatch);
