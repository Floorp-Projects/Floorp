/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This module provides temporary shim until Bug 843901 is shipped.
// It basically registers tab event listeners on all windows that get
// opened and forwards them through observer notifications.

module.metadata = {
  "stability": "experimental"
};

const { Ci } = require("chrome");
const { windows, isInteractive } = require("../window/utils");
const { events } = require("../browser/events");
const { open } = require("../event/dom");
const { filter, map, merge, expand } = require("../event/utils");
const isFennec = require("sdk/system/xul-app").is("Fennec");

// Module provides event stream (in nodejs style) that emits data events
// for all the tab events that happen in running firefox. At the moment
// it does it by registering listeners on all browser windows and then
// forwarding events when they occur to a stream. This will become obsolete
// once Bug 843901 is fixed, and we'll just leverage observer notifications.

// Set of tab events that this module going to aggregate and expose.
const TYPES = ["TabOpen","TabClose","TabSelect","TabMove","TabPinned",
               "TabUnpinned"];

// Utility function that given a browser `window` returns stream of above
// defined tab events for all tabs on the given window.
function tabEventsFor(window) {
  // Map supported event types to a streams of those events on the given
  // `window` and than merge these streams into single form stream off
  // all events.
  let channels = TYPES.map(type => open(window, type));
  return merge(channels);
}

// Create our event channels.  We do this in a separate function to
// minimize the chance of leaking intermediate objects on the global.
function makeEvents() {
  // Filter DOMContentLoaded events from all the browser events.
  var readyEvents = filter(events, e => e.type === "DOMContentLoaded");
  // Map DOMContentLoaded events to it's target browser windows.
  var futureWindows = map(readyEvents, e => e.target);
  // Expand all browsers that will become interactive to supported tab events
  // on these windows. Result will be a tab events from all tabs of all windows
  // that will become interactive.
  var eventsFromFuture = expand(futureWindows, tabEventsFor);

  // Above covers only windows that will become interactive in a future, but some
  // windows may already be interactive so we pick those and expand to supported
  // tab events for them too.
  var interactiveWindows = windows("navigator:browser", { includePrivate: true }).
                           filter(isInteractive);
  var eventsFromInteractive = merge(interactiveWindows.map(tabEventsFor));


  // Finally merge stream of tab events from future windows and current windows
  // to cover all tab events on all windows that will open.
  return merge([eventsFromInteractive, eventsFromFuture]);
}

// Map events to Fennec format if necessary
exports.events = map(makeEvents(), function (event) {
  return !isFennec ? event : {
    type: event.type,
    target: event.target.ownerDocument.defaultView.BrowserApp
            .getTabForBrowser(event.target)
  };
});
