/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Trait } = require("../deprecated/traits");
const { EventEmitter } = require("../deprecated/events");
const { defer } = require("../lang/functional");
const { EVENTS } = require("./events");
const { getThumbnailURIForWindow } = require("../content/thumbnail");
const { getFaviconURIForLocation } = require("../io/data");
const { activateTab, getOwnerWindow, getBrowserForTab, getTabTitle, setTabTitle,
        getTabURL, setTabURL, getTabContentType, getTabId } = require('./utils');

// Array of the inner instances of all the wrapped tabs.
const TABS = [];

/**
 * Trait used to create tab wrappers.
 */
const TabTrait = Trait.compose(EventEmitter, {
  on: Trait.required,
  _emit: Trait.required,
  /**
   * Tab DOM element that is being wrapped.
   */
  _tab: null,
  /**
   * Window wrapper whose tab this object represents.
   */
  window: null,
  constructor: function Tab(options) {
    this._onReady = this._onReady.bind(this);
    this._tab = options.tab;
    // TODO: Remove this dependency
    let window = this.window = options.window || require('../windows').BrowserWindow({ window: getOwnerWindow(this._tab) });

    // Setting event listener if was passed.
    for each (let type in EVENTS) {
      let listener = options[type.listener];
      if (listener)
        this.on(type.name, options[type.listener]);
      if ('ready' != type.name) // window spreads this event.
        window.tabs.on(type.name, this._onEvent.bind(this, type.name));
    }

    this.on(EVENTS.close.name, this.destroy.bind(this));
    this._browser.addEventListener(EVENTS.ready.dom, this._onReady, true);

    if (options.isPinned)
      this.pin();

    // Since we will have to identify tabs by a DOM elements facade function
    // is used as constructor that collects all the instances and makes sure
    // that they more then one wrapper is not created per tab.
    return this;
  },
  destroy: function destroy() {
    this._removeAllListeners();
    this._browser.removeEventListener(EVENTS.ready.dom, this._onReady, true);
  },

  /**
   * Internal listener that emits public event 'ready' when the page of this
   * tab is loaded.
   */
  _onReady: function _onReady(event) {
    // IFrames events will bubble so we need to ignore those.
    if (event.target == this._contentDocument)
      this._emit(EVENTS.ready.name, this._public);
  },
  /**
   * Internal tab event router. Window will emit tab related events for all it's
   * tabs, this listener will propagate all the events for this tab to it's
   * listeners.
   */
  _onEvent: function _onEvent(type, tab) {
    if (tab == this._public)
      this._emit(type, tab);
  },
  /**
   * Browser DOM element where page of this tab is currently loaded.
   */
  get _browser() getBrowserForTab(this._tab),
  /**
   * Window DOM element containing this tab.
   */
  get _window() getOwnerWindow(this._tab),
  /**
   * Document object of the page that is currently loaded in this tab.
   */
  get _contentDocument() this._browser.contentDocument,
  /**
   * Window object of the page that is currently loaded in this tab.
   */
  get _contentWindow() this._browser.contentWindow,

  /**
   * Unique id for the tab, actually maps to tab.linkedPanel but with some munging.
   */
  get id() getTabId(this._tab),

  /**
   * The title of the page currently loaded in the tab.
   * Changing this property changes an actual title.
   * @type {String}
   */
  get title() getTabTitle(this._tab),
  set title(title) setTabTitle(this._tab, title),

  /**
   * Returns the MIME type that the document loaded in the tab is being
   * rendered as.
   * @type {String}
   */
  get contentType() getTabContentType(this._tab),

  /**
   * Location of the page currently loaded in this tab.
   * Changing this property will loads page under under the specified location.
   * @type {String}
   */
  get url() getTabURL(this._tab),
  set url(url) setTabURL(this._tab, url),
  /**
   * URI of the favicon for the page currently loaded in this tab.
   * @type {String}
   */
  get favicon() getFaviconURIForLocation(this.url),
  /**
   * The CSS style for the tab
   */
  get style() null, // TODO
  /**
   * The index of the tab relative to other tabs in the application window.
   * Changing this property will change order of the actual position of the tab.
   * @type {Number}
   */
  get index()
    this._window.gBrowser.getBrowserIndexForDocument(this._contentDocument),
  set index(value) this._window.gBrowser.moveTabTo(this._tab, value),
  /**
   * Thumbnail data URI of the page currently loaded in this tab.
   * @type {String}
   */
  getThumbnail: function getThumbnail()
    getThumbnailURIForWindow(this._contentWindow),
  /**
   * Whether or not tab is pinned (Is an app-tab).
   * @type {Boolean}
   */
  get isPinned() this._tab.pinned,
  pin: function pin() {
    this._window.gBrowser.pinTab(this._tab);
  },
  unpin: function unpin() {
    this._window.gBrowser.unpinTab(this._tab);
  },

  /**
   * Create a worker for this tab, first argument is options given to Worker.
   * @type {Worker}
   */
  attach: function attach(options) {
    // BUG 792946 https://bugzilla.mozilla.org/show_bug.cgi?id=792946
    // TODO: fix this circular dependency
    let { Worker } = require('./worker');
    return Worker(options, this._contentWindow);
  },

  /**
   * Make this tab active.
   * Please note: That this function is called asynchronous since in E10S that
   * will be the case. Besides this function is called from a constructor where
   * we would like to return instance before firing a 'TabActivated' event.
   */
  activate: defer(function activate() {
    activateTab(this._tab);
  }),
  /**
   * Close the tab
   */
  close: function close(callback) {
    if (callback)
      this.once(EVENTS.close.name, callback);
    this._window.gBrowser.removeTab(this._tab);
  },
  /**
   * Reload the tab
   */
  reload: function reload() {
    this._window.gBrowser.reloadTab(this._tab);
  }
});

function Tab(options) {
  let chromeTab = options.tab;
  for each (let tab in TABS) {
    if (chromeTab == tab._tab)
      return tab._public;
  }
  let tab = TabTrait(options);
  TABS.push(tab);
  return tab._public;
}
Tab.prototype = TabTrait.prototype;
exports.Tab = Tab;
