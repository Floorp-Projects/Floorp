/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { EventTarget } = require("../event/target");
const { emit } = require("../event/core");
const { WindowTracker, windowIterator } = require("../deprecated/window-utils");
const { DOMEventAssembler } = require("../deprecated/events/assembler");
const { Class } = require("../core/heritage");
const { Cu } = require("chrome");

// Event emitter objects used to register listeners and emit events on them
// when they occur.
const Observer = Class({
  initialize() {
    // Using `WindowTracker` to track window events.
    WindowTracker({
      onTrack: chromeWindow => {
        emit(this, "open", chromeWindow);
        this.observe(chromeWindow);
      },
      onUntrack: chromeWindow => {
        emit(this, "close", chromeWindow);
        this.ignore(chromeWindow);
      }
    });
  },
  implements: [EventTarget, DOMEventAssembler],
  /**
   * Events that are supported and emitted by the module.
   */
  supportedEventsTypes: [ "activate", "deactivate" ],
  /**
   * Function handles all the supported events on all the windows that are
   * observed. Method is used to proxy events to the listeners registered on
   * this event emitter.
   * @param {Event} event
   *    Keyboard event being emitted.
   */
  handleEvent(event) {
    // Ignore events from windows in the child process as they can't be top-level
    if (Cu.isCrossProcessWrapper(event.target))
      return;
    emit(this, event.type, event.target, event);
  }
});

exports.observer = new Observer();
