/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "unstable"
};

const { EventEmitterTrait: EventEmitter } = require("../deprecated/events");
const { DOMEventAssembler } = require("../deprecated/events/assembler");
const { Trait } = require("../deprecated/light-traits");
const { getActiveTab, getTabs, getTabContainer } = require("./utils");
const { browserWindowIterator } = require("../deprecated/window-utils");
const { isBrowser } = require('../window/utils');
const { observer: windowObserver } = require("../windows/observer");

const EVENTS = {
  "TabOpen": "open",
  "TabClose": "close",
  "TabSelect": "select",
  "TabMove": "move",
  "TabPinned": "pinned",
  "TabUnpinned": "unpinned"
};


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
  supportedEventsTypes: Object.keys(EVENTS),
  /**
   * Function handles all the supported events on all the windows that are
   * observed. Method is used to proxy events to the listeners registered on
   * this event emitter.
   * @param {Event} event
   *    Keyboard event being emitted.
   */
  handleEvent: function handleEvent(event) {
    this._emit(EVENTS[event.type], event.target, event);
  }
});

// Currently Gecko does not dispatch any event on the previously selected
// tab before / after "TabSelect" is dispatched. In order to work around this
// limitation we keep track of selected tab and emit "deactivate" event with
// that before emitting "activate" on selected tab.
var selectedTab = null;
function onTabSelect(tab) {
  if (selectedTab !== tab) {
    if (selectedTab) observer._emit('deactivate', selectedTab);
    if (tab) observer._emit('activate', selectedTab = tab);
  }
};
observer.on('select', onTabSelect);

// We also observe opening / closing windows in order to add / remove it's
// containers to the observed list.
function onWindowOpen(chromeWindow) {
  if (!isBrowser(chromeWindow)) return; // Ignore if it's not a browser window.
  observer.observe(getTabContainer(chromeWindow));
}
windowObserver.on("open", onWindowOpen);

function onWindowClose(chromeWindow) {
  if (!isBrowser(chromeWindow)) return; // Ignore if it's not a browser window.
  // Bug 751546: Emit `deactivate` event on window close immediatly
  // Otherwise we are going to face "dead object" exception on `select` event
  if (getActiveTab(chromeWindow) == selectedTab) {
    observer._emit("deactivate", selectedTab);
    selectedTab = null;
  }
  observer.ignore(getTabContainer(chromeWindow));
}
windowObserver.on("close", onWindowClose);


// Currently gecko does not dispatches "TabSelect" events when different
// window gets activated. To work around this limitation we emulate "select"
// event for this case.
windowObserver.on("activate", function onWindowActivate(chromeWindow) {
  if (!isBrowser(chromeWindow)) return; // Ignore if it's not a browser window.
  observer._emit("select", getActiveTab(chromeWindow));
});

// We should synchronize state, since probably we already have at least one
// window open.
for each (let window in browserWindowIterator()) onWindowOpen(window);

exports.observer = observer;
