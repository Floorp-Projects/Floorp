/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "unstable"
};

const { EventTarget } = require("../event/target");
const { emit } = require("../event/core");
const { DOMEventAssembler } = require("../deprecated/events/assembler");
const { Class } = require("../core/heritage");
const { getActiveTab, getTabs } = require("./utils");
const { browserWindowIterator } = require("../deprecated/window-utils");
const { isBrowser, windows, getMostRecentBrowserWindow } = require("../window/utils");
const { observer: windowObserver } = require("../windows/observer");

const EVENTS = {
  "TabOpen": "open",
  "TabClose": "close",
  "TabSelect": "select",
  "TabMove": "move",
  "TabPinned": "pinned",
  "TabUnpinned": "unpinned"
};

const selectedTab = Symbol("observer/state/selectedTab");

// Event emitter objects used to register listeners and emit events on them
// when they occur.
const Observer = Class({
  implements: [EventTarget, DOMEventAssembler],
  initialize() {
    this[selectedTab] = null;
    // Currently Gecko does not dispatch any event on the previously selected
    // tab before / after "TabSelect" is dispatched. In order to work around this
    // limitation we keep track of selected tab and emit "deactivate" event with
    // that before emitting "activate" on selected tab.
    this.on("select", tab => {
      const selected = this[selectedTab];
      if (selected !== tab) {
        if (selected) {
          emit(this, 'deactivate', selected);
        }

        if (tab) {
          this[selectedTab] = tab;
          emit(this, 'activate', this[selectedTab]);
        }
      }
    });


    // We also observe opening / closing windows in order to add / remove it's
    // containers to the observed list.
    windowObserver.on("open", chromeWindow => {
      if (isBrowser(chromeWindow)) {
        this.observe(chromeWindow);
      }
    });

    windowObserver.on("close", chromeWindow => {
      if (isBrowser(chromeWindow)) {
        // Bug 751546: Emit `deactivate` event on window close immediatly
        // Otherwise we are going to face "dead object" exception on `select` event
        if (getActiveTab(chromeWindow) === this[selectedTab]) {
          emit(this, "deactivate", this[selectedTab]);
          this[selectedTab] = null;
        }
        this.ignore(chromeWindow);
      }
    });


    // Currently gecko does not dispatches "TabSelect" events when different
    // window gets activated. To work around this limitation we emulate "select"
    // event for this case.
    windowObserver.on("activate", chromeWindow => {
      if (isBrowser(chromeWindow)) {
        emit(this, "select", getActiveTab(chromeWindow));
      }
    });

    // We should synchronize state, since probably we already have at least one
    // window open.
    for (let chromeWindow of browserWindowIterator()) {
      this.observe(chromeWindow);
    }
  },
  /**
   * Events that are supported and emitted by the module.
   */
  supportedEventsTypes: Object.keys(EVENTS),
  /**
   * Function handles all the supported events on all the windows that are
   * observed. Method is used to proxy events to the listeners registered on
   * this event emitter.
   * @param {Event} event
   *    Keyboard event being emitted.
   */
  handleEvent: function handleEvent(event) {
    emit(this, EVENTS[event.type], event.target, event);
  }
});

exports.observer = new Observer();
