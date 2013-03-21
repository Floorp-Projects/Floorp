/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Trait } = require("../deprecated/traits");
const { List } = require("../deprecated/list");
const { Tab } = require("../tabs/tab");
const { EventEmitter } = require("../deprecated/events");
const { EVENTS } = require("../tabs/events");
const { getOwnerWindow, getActiveTab, getTabs,
        openTab } = require("../tabs/utils");
const { Options } = require("../tabs/common");
const { observer: tabsObserver } = require("../tabs/observer");
const { ignoreWindow, isWindowPrivate } = require("../private-browsing/utils");

const TAB_BROWSER = "tabbrowser";

/**
 * This is a trait that is used in composition of window wrapper. Trait tracks
 * tab related events of the wrapped window in order to keep track of open
 * tabs and maintain their wrappers. Every new tab gets wrapped and jetpack
 * type event is emitted.
 */
const WindowTabTracker = Trait.compose({
  /**
   * Chrome window whose tabs are tracked.
   */
  _window: Trait.required,
  /**
   * Function used to emit events.
   */
  _emit: EventEmitter.required,
  _tabOptions: Trait.required,
  /**
   * Function to add event listeners.
   */
  on: EventEmitter.required,
  removeListener: EventEmitter.required,
  /**
   * Initializes tab tracker for a browser window.
   */
  _initWindowTabTracker: function _initWindowTabTracker() {
    // Ugly hack that we have to remove at some point (see Bug 658059). At this
    // point it is necessary to invoke lazy `tabs` getter on the windows object
    // which creates a `TabList` instance.
    this.tabs;

    // Binding all methods used as event listeners to the instance.
    this._onTabReady = this._emitEvent.bind(this, "ready");
    this._onTabOpen = this._onTabEvent.bind(this, "open");
    this._onTabClose = this._onTabEvent.bind(this, "close");
    this._onTabActivate = this._onTabEvent.bind(this, "activate");
    this._onTabDeactivate = this._onTabEvent.bind(this, "deactivate");
    this._onTabPinned = this._onTabEvent.bind(this, "pinned");
    this._onTabUnpinned = this._onTabEvent.bind(this, "unpinned");

    for each (let tab in getTabs(this._window)) {
      // We emulate "open" events for all open tabs since gecko does not emits
      // them on the tabs that new windows are open with. Also this is
      // necessary to synchronize tabs lists with an actual state.
      this._onTabOpen(tab);
    }

    // We also emulate "activate" event so that it's picked up by a tab list.
    this._onTabActivate(getActiveTab(this._window));

    // Setting up event listeners
    tabsObserver.on("open", this._onTabOpen);
    tabsObserver.on("close", this._onTabClose);
    tabsObserver.on("activate", this._onTabActivate);
    tabsObserver.on("deactivate", this._onTabDeactivate);
    tabsObserver.on("pinned", this._onTabPinned);
    tabsObserver.on("unpinned", this._onTabUnpinned);
  },
  _destroyWindowTabTracker: function _destroyWindowTabTracker() {
    // We emulate close events on all tabs, since gecko does not emits such
    // events by itself.
    for each (let tab in this.tabs)
      this._emitEvent("close", tab);

    this._tabs._clear();

    tabsObserver.removeListener("open", this._onTabOpen);
    tabsObserver.removeListener("close", this._onTabClose);
    tabsObserver.removeListener("activate", this._onTabActivate);
    tabsObserver.removeListener("deactivate", this._onTabDeactivate);
  },
  _onTabEvent: function _onTabEvent(type, tab) {
    // Accept only tabs for the watched window, and ignore private tabs
    // if addon doesn't have private permission
    if (this._window === getOwnerWindow(tab) && !ignoreWindow(this._window)) {
      let options = this._tabOptions.shift() || {};
      options.tab = tab;
      options.window = this._public;

      // Ignore zombie tabs on open that have already been removed
      if (type == "open" && !tab.linkedBrowser)
        return;

      // Create a tab wrapper on open event, otherwise, just fetch existing
      // tab object
      let wrappedTab = Tab(options, type !== "open");
      if (!wrappedTab)
        return;

      // Setting up an event listener for ready events.
      if (type === "open")
        wrappedTab.on("ready", this._onTabReady);

      this._emitEvent(type, wrappedTab);
    }
  },
  _emitEvent: function _emitEvent(type, tab) {
    // Notifies combined tab list that tab was added / removed.
    tabs._emit(type, tab);
    // Notifies contained tab list that window was added / removed.
    this._tabs._emit(type, tab);
  }
});
exports.WindowTabTracker = WindowTabTracker;

/**
 * This trait is used to create live representation of open tab lists. Each
 * window wrapper's tab list is represented by an object created from this
 * trait. It is also used to represent list of all the open windows. Trait is
 * composed out of `EventEmitter` in order to emit 'TabOpen', 'TabClose' events.
 * **Please note** that objects created by this trait can't be exposed outside
 * instead you should expose it's `_public` property, see comments in
 * constructor for details.
 */
const TabList = List.resolve({ constructor: "_init" }).compose(
  // This is ugly, but necessary. Will be removed by #596248
  EventEmitter.resolve({ toString: null }),
  Trait.compose({
    on: Trait.required,
    _emit: Trait.required,
    constructor: function TabList(options) {
      this._window = options.window;
      // Add new items to the list
      this.on(EVENTS.open.name, this._add.bind(this));

      // Remove closed items from the list
      this.on(EVENTS.close.name, this._remove.bind(this));

      // Set value whenever new tab becomes active.
      this.on("activate", function onTabActivate(tab) {
        this._activeTab = tab;
      }.bind(this));

      // Initialize list.
      this._init();
      // This list is not going to emit any events, object holding this list
      // will do it instead, to make that possible we return a private API.
      return this;
    },
    get activeTab() this._activeTab,
    _activeTab: null,

    open: function open(options) {
      let window = this._window;
      let chromeWindow = window._window;
      options = Options(options);

      // save the tab options
      window._tabOptions.push(options);
      // open the tab
      let tab = openTab(chromeWindow, options.url, options);
    }
  // This is ugly, but necessary. Will be removed by #596248
  }).resolve({ toString: null })
);

/**
 * Combined list of all open tabs on all the windows.
 * type {TabList}
 */
var tabs = TabList({ window: null });
exports.tabs = tabs._public;

/**
 * Trait is a part of composition that represents window wrapper. This trait is
 * composed out of `WindowTabTracker` that allows it to keep track of open tabs
 * on the window being wrapped.
 */
const WindowTabs = Trait.compose(
  WindowTabTracker,
  Trait.compose({
    _window: Trait.required,
    /**
     * List of tabs
     */
    get tabs() (this._tabs || (this._tabs = TabList({ window: this })))._public,
    _tabs: null,
  })
);
exports.WindowTabs = WindowTabs;
