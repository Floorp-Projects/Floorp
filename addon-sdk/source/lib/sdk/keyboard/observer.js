/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Class } = require("../core/heritage");
const { EventTarget } = require("../event/target");
const { emit } = require("../event/core");
const { DOMEventAssembler } = require("../deprecated/events/assembler");
const { browserWindowIterator } = require('../deprecated/window-utils');
const { isBrowser } = require('../window/utils');
const { observer: windowObserver } = require("../windows/observer");

// Event emitter objects used to register listeners and emit events on them
// when they occur.
const Observer = Class({
  implements: [DOMEventAssembler, EventTarget],
  initialize() {
    // Adding each opened window to a list of observed windows.
    windowObserver.on("open", window => {
      if (isBrowser(window))
        this.observe(window);
    });

    // Removing each closed window form the list of observed windows.
    windowObserver.on("close", window => {
      if (isBrowser(window))
        this.ignore(window);
    });

    // Making observer aware of already opened windows.
    for (let window of browserWindowIterator()) {
      this.observe(window);
    }
  },
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
  handleEvent(event) {
    emit(this, event.type, event, event.target.ownerDocument.defaultView);
  }
});

exports.observer = new Observer();
