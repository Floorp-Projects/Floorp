/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Trait } = require("../deprecated/light-traits");
const { EventEmitterTrait: EventEmitter } = require("../deprecated/events");
const { DOMEventAssembler } = require("../deprecated/events/assembler");
const { browserWindowIterator } = require('../deprecated/window-utils');
const { isBrowser } = require('../window/utils');
const { observer: windowObserver } = require("../windows/observer");

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
  supportedEventsTypes: [ "keydown", "keyup", "keypress" ],
  /**
   * Function handles all the supported events on all the windows that are
   * observed. Method is used to proxy events to the listeners registered on
   * this event emitter.
   * @param {Event} event
   *    Keyboard event being emitted.
   */
  handleEvent: function handleEvent(event) {
    this._emit(event.type, event, event.target.ownerDocument.defaultView);
  }
});

// Adding each opened window to a list of observed windows.
windowObserver.on("open", function onOpen(window) {
  if (isBrowser(window))
    observer.observe(window);
});
// Removing each closed window form the list of observed windows.
windowObserver.on("close", function onClose(window) {
  if (isBrowser(window))
    observer.ignore(window);
});

// Making observer aware of already opened windows.
for each (let window in browserWindowIterator())
  observer.observe(window);

exports.observer = observer;
