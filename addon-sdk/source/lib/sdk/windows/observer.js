/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { EventEmitterTrait: EventEmitter } = require("../deprecated/events");
const { WindowTracker, windowIterator } = require("../deprecated/window-utils");
const { DOMEventAssembler } = require("../deprecated/events/assembler");
const { Trait } = require("../deprecated/light-traits");

// Event emitter objects used to register listeners and emit events on them
// when they occur.
const observer = Trait.compose(DOMEventAssembler, EventEmitter).create({
  /**
   * Method is implemented by `EventEmitter` and is used just for emitting
   * events on registered listeners.
   */
  _emit: Trait.required,
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
  handleEvent: function handleEvent(event) {
    this._emit(event.type, event.target, event);
  }
});

// Using `WindowTracker` to track window events.
WindowTracker({
  onTrack: function onTrack(chromeWindow) {
    observer._emit("open", chromeWindow);
    observer.observe(chromeWindow);
  },
  onUntrack: function onUntrack(chromeWindow) {
    observer._emit("close", chromeWindow);
    observer.ignore(chromeWindow);
  }
});

exports.observer = observer;
