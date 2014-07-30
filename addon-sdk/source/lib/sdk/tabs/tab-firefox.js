 /*This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Trait } = require("../deprecated/traits");
const { EventEmitter } = require("../deprecated/events");
const { defer } = require("../lang/functional");
const { has } = require("../util/array");
const { each } = require("../util/object");
const { EVENTS } = require("./events");
const { getThumbnailURIForWindow } = require("../content/thumbnail");
const { getFaviconURIForLocation } = require("../io/data");
const { activateTab, getOwnerWindow, getBrowserForTab, getTabTitle,
        setTabTitle, getTabContentDocument, getTabURL, setTabURL,
        getTabContentType, getTabId } = require('./utils');
const { isPrivate } = require('../private-browsing/utils');
const { isWindowPrivate } = require('../window/utils');
const viewNS = require('../core/namespace').ns();
const { deprecateUsage } = require('../util/deprecate');
const { getURL } = require('../url/utils');
const { viewFor } = require('../view/core');
const { observer } = require('./observer');

// cfx doesn't know require() now handles JSM modules
const FRAMESCRIPT_MANAGER = '../../framescript/FrameScriptManager.jsm';
require(FRAMESCRIPT_MANAGER).enableTabEvents();

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
    this._tab = options.tab;
    // TODO: Remove this dependency
    let window = this.window = options.window || require('../windows').BrowserWindow({ window: getOwnerWindow(this._tab) });

    // Setting event listener if was passed.
    each(EVENTS, (type) => {
      let listener = options[type.listener];
      if (listener) {
        this.on(type.name, options[type.listener]);
      }
      // window spreads this event.
      if (!has(['ready', 'load', 'pageshow'], (type.name)))
        window.tabs.on(type.name, this._onEvent.bind(this, type.name));
    });

    this.on(EVENTS.close.name, this.destroy.bind(this));

    this._onContentEvent = this._onContentEvent.bind(this);
    this._browser.messageManager.addMessageListener('sdk/tab/event', this._onContentEvent);

    // bug 1024632 - first tab inNewWindow gets events from the synthetic 
    // about:blank document. ignore them unless that is the actual target url.
    this._skipBlankEvents = options.inNewWindow && options.url !== 'about:blank';

    if (options.isPinned)
      this.pin();

    viewNS(this._public).tab = this._tab;
    viewFor.implement(this._public, getTabView);
    isPrivate.implement(this._public, tab => isWindowPrivate(getChromeTab(tab)));

    // Add tabs to getURL method
    getURL.implement(this._public, (function (obj) this._public.url).bind(this));

    // Since we will have to identify tabs by a DOM elements facade function
    // is used as constructor that collects all the instances and makes sure
    // that they more then one wrapper is not created per tab.
    return this;
  },
  destroy: function destroy() {
    this._removeAllListeners();
    if (this._tab) {
      let browser = this._browser;
      // The tab may already be removed from DOM -or- not yet added
      if (browser) {
        browser.messageManager.removeMessageListener('sdk/tab/event', this._onContentEvent);
      }
      this._tab = null;
      TABS.splice(TABS.indexOf(this), 1);
    }
  },

  /**
   * internal message listener emits public events (ready, load and pageshow)
   * forwarded from content frame script tab-event.js
   */
  _onContentEvent: function({ data }) {
    // bug 1024632 - skip initial events from synthetic about:blank document
    if (this._skipBlankEvents && this.window.tabs.length === 1 && this.url === 'about:blank')
      return;

    // first time we don't skip blank events, disable further skipping
    this._skipBlankEvents = false;

    this._emit(data.type, this._public, data.persisted);
  },

  /**
   * Internal tab event router. Window will emit tab related events for all it's
   * tabs, this listener will propagate all the events for this tab to it's
   * listeners.
   */
  _onEvent: function _onEvent(type, tab) {
    if (viewNS(tab).tab == this._tab)
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
  get _contentDocument() getTabContentDocument(this._tab),
  /**
   * Window object of the page that is currently loaded in this tab.
   */
  get _contentWindow() this._browser.contentWindow,

  /**
   * tab's document readyState, or 'uninitialized' if it doesn't even exist yet.
   */
  get readyState() {
    let doc = this._contentDocument;
    return doc && doc.readyState || 'uninitialized';
  },

  /**
   * Unique id for the tab, actually maps to tab.linkedPanel but with some munging.
   */
  get id() this._tab ? getTabId(this._tab) : undefined,

  /**
   * The title of the page currently loaded in the tab.
   * Changing this property changes an actual title.
   * @type {String}
   */
  get title() this._tab ? getTabTitle(this._tab) : undefined,
  set title(title) this._tab && setTabTitle(this._tab, title),

  /**
   * Returns the MIME type that the document loaded in the tab is being
   * rendered as.
   * @type {String}
   */
  get contentType() this._tab ? getTabContentType(this._tab) : undefined,

  /**
   * Location of the page currently loaded in this tab.
   * Changing this property will loads page under under the specified location.
   * @type {String}
   */
  get url() this._tab ? getTabURL(this._tab) : undefined,
  set url(url) this._tab && setTabURL(this._tab, url),
  /**
   * URI of the favicon for the page currently loaded in this tab.
   * @type {String}
   */
  get favicon() {
    deprecateUsage(
      'tab.favicon is deprecated, ' +
      'please use require("sdk/places/favicon").getFavicon instead.'
    );
    return this._tab ? getFaviconURIForLocation(this.url) : undefined
  },
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
    this._tab ?
    this._window.gBrowser.getBrowserIndexForDocument(this._contentDocument) :
    undefined,
  set index(value)
    this._tab && this._window.gBrowser.moveTabTo(this._tab, value),
  /**
   * Thumbnail data URI of the page currently loaded in this tab.
   * @type {String}
   */
  getThumbnail: function getThumbnail()
    this._tab ? getThumbnailURIForWindow(this._contentWindow) : undefined,
  /**
   * Whether or not tab is pinned (Is an app-tab).
   * @type {Boolean}
   */
  get isPinned() this._tab ? this._tab.pinned : undefined,
  pin: function pin() {
    if (!this._tab)
      return;
    this._window.gBrowser.pinTab(this._tab);
  },
  unpin: function unpin() {
    if (!this._tab)
      return;
    this._window.gBrowser.unpinTab(this._tab);
  },

  /**
   * Create a worker for this tab, first argument is options given to Worker.
   * @type {Worker}
   */
  attach: function attach(options) {
    if (!this._tab)
      return;
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
    if (!this._tab)
      return;
    activateTab(this._tab);
  }),
  /**
   * Close the tab
   */
  close: function close(callback) {
    // Bug 699450: the tab may already have been detached
    if (!this._tab || !this._tab.parentNode) {
      if (callback)
        callback();
      return;
    }
    if (callback) {
      if (this.window.tabs.activeTab && (this.window.tabs.activeTab.id == this.id))
        observer.once('select', callback);
      else
        this.once(EVENTS.close.name, callback);
    }
    this._window.gBrowser.removeTab(this._tab);
  },
  /**
   * Reload the tab
   */
  reload: function reload() {
    if (!this._tab)
      return;
    this._window.gBrowser.reloadTab(this._tab);
  }
});

function getChromeTab(tab) {
  return getOwnerWindow(viewNS(tab).tab);
}

// Implement `viewFor` polymorphic function for the Tab
// instances.
const getTabView = tab => viewNS(tab).tab;

function Tab(options, existingOnly) {
  let chromeTab = options.tab;
  for (let tab of TABS) {
    if (chromeTab == tab._tab)
      return tab._public;
  }
  // If called asked to return only existing wrapper,
  // we should return null here as no matching Tab object has been found
  if (existingOnly)
    return null;

  let tab = TabTrait(options);
  TABS.push(tab);
  return tab._public;
}
Tab.prototype = TabTrait.prototype;
exports.Tab = Tab;
