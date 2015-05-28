/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cc, Ci } = require('chrome');
const { Class } = require('../core/heritage');
const { tabNS, rawTabNS } = require('./namespace');
const { EventTarget } = require('../event/target');
const { activateTab, getTabTitle, setTabTitle, closeTab, getTabURL,
        getTabContentWindow, getTabForBrowser, setTabURL, getOwnerWindow,
        getTabContentDocument, getTabContentType, getTabId, isTab } = require('./utils');
const { emit } = require('../event/core');
const { isPrivate } = require('../private-browsing/utils');
const { isWindowPrivate } = require('../window/utils');
const { when: unload } = require('../system/unload');
const { BLANK } = require('../content/thumbnail');
const { viewFor } = require('../view/core');
const { EVENTS } = require('./events');
const { modelFor } = require('../model/core');

const ERR_FENNEC_MSG = 'This method is not yet supported by Fennec';

const Tab = Class({
  extends: EventTarget,
  initialize: function initialize(options) {
    options = options.tab ? options : { tab: options };
    let tab = options.tab;

    EventTarget.prototype.initialize.call(this, options);
    let tabInternals = tabNS(this);
    rawTabNS(tab).tab = this;

    let window = tabInternals.window = options.window || getOwnerWindow(tab);
    tabInternals.tab = tab;

    // TabReady
    let onReady = tabInternals.onReady = onTabReady.bind(this);
    tab.browser.addEventListener(EVENTS.ready.dom, onReady, false);

    // TabPageShow
    let onPageShow = tabInternals.onPageShow = onTabPageShow.bind(this);
    tab.browser.addEventListener(EVENTS.pageshow.dom, onPageShow, false);

    // TabLoad
    let onLoad = tabInternals.onLoad = onTabLoad.bind(this);
    tab.browser.addEventListener(EVENTS.load.dom, onLoad, true);

    // TabClose
    let onClose = tabInternals.onClose = onTabClose.bind(this);
    window.BrowserApp.deck.addEventListener(EVENTS.close.dom, onClose, false);

    unload(cleanupTab.bind(null, this));
  },

  /**
   * The title of the page currently loaded in the tab.
   * Changing this property changes an actual title.
   * @type {String}
   */
  get title() getTabTitle(tabNS(this).tab),
  set title(title) setTabTitle(tabNS(this).tab, title),

  /**
   * Location of the page currently loaded in this tab.
   * Changing this property will loads page under under the specified location.
   * @type {String}
   */
  get url() {
    return tabNS(this).closed ? undefined : getTabURL(tabNS(this).tab);
  },
  set url(url) setTabURL(tabNS(this).tab, url),

  /**
   * URI of the favicon for the page currently loaded in this tab.
   * @type {String}
   */
  get favicon() {
    /*
     * Synchronous favicon services were never supported on Fennec,
     * and as of FF22, are now deprecated. When/if favicon services
     * are supported for Fennec, this getter should reference
     * `require('sdk/places/favicon').getFavicon`
     */
    console.error(
      'tab.favicon is deprecated, and currently ' +
      'favicon helpers are not yet supported by Fennec'
    );

    // return 16x16 blank default
    return 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAEklEQVQ4jWNgGAWjYBSMAggAAAQQAAF/TXiOAAAAAElFTkSuQmCC';
  },

  getThumbnail: function() {
    // TODO: implement!
    console.error(ERR_FENNEC_MSG);

    // return 80x45 blank default
    return BLANK;
  },

  /**
   * tab's document readyState, or 'uninitialized' if it doesn't even exist yet.
   */
  get readyState() {
    let doc = getTabContentDocument(tabNS(this).tab);
    return doc && doc.readyState || 'uninitialized';
  },

  get id() {
    return getTabId(tabNS(this).tab);
  },

  /**
   * The index of the tab relative to other tabs in the application window.
   * Changing this property will change order of the actual position of the tab.
   * @type {Number}
   */
  get index() {
    if (tabNS(this).closed) return undefined;

    let tabs = tabNS(this).window.BrowserApp.tabs;
    let tab = tabNS(this).tab;
    for (var i = tabs.length; i >= 0; i--) {
      if (tabs[i] === tab)
        return i;
    }
    return null;
  },
  set index(value) {
    console.error(ERR_FENNEC_MSG); // TODO
  },

  /**
   * Whether or not tab is pinned (Is an app-tab).
   * @type {Boolean}
   */
  get isPinned() {
    console.error(ERR_FENNEC_MSG); // TODO
    return false; // TODO
  },
  pin: function pin() {
    console.error(ERR_FENNEC_MSG); // TODO
  },
  unpin: function unpin() {
    console.error(ERR_FENNEC_MSG); // TODO
  },

  /**
   * Returns the MIME type that the document loaded in the tab is being
   * rendered as.
   * @type {String}
   */
  get contentType() getTabContentType(tabNS(this).tab),

  /**
   * Create a worker for this tab, first argument is options given to Worker.
   * @type {Worker}
   */
  attach: function attach(options) {
    // BUG 792946 https://bugzilla.mozilla.org/show_bug.cgi?id=792946
    // TODO: fix this circular dependency
    let { Worker } = require('./worker');
    return Worker(options, getTabContentWindow(tabNS(this).tab));
  },

  /**
   * Make this tab active.
   */
  activate: function activate() {
    activateTab(tabNS(this).tab, tabNS(this).window);
  },

  /**
   * Close the tab
   */
  close: function close(callback) {
    let tab = this;
    this.once(EVENTS.close.name, function () {
      tabNS(tab).closed = true;
      if (callback) callback();
    });

    closeTab(tabNS(this).tab);
  },

  /**
   * Reload the tab
   */
  reload: function reload() {
    tabNS(this).tab.browser.reload();
  }
});
exports.Tab = Tab;

// Implement `viewFor` polymorphic function for the Tab
// instances.
viewFor.define(Tab, x => tabNS(x).tab);

function cleanupTab(tab) {
  let tabInternals = tabNS(tab);
  if (!tabInternals.tab)
    return;

  if (tabInternals.tab.browser) {
    tabInternals.tab.browser.removeEventListener(EVENTS.ready.dom, tabInternals.onReady, false);
    tabInternals.tab.browser.removeEventListener(EVENTS.pageshow.dom, tabInternals.onPageShow, false);
    tabInternals.tab.browser.removeEventListener(EVENTS.load.dom, tabInternals.onLoad, true);
  }
  tabInternals.onReady = null;
  tabInternals.onPageShow = null;
  tabInternals.onLoad = null;
  tabInternals.window.BrowserApp.deck.removeEventListener(EVENTS.close.dom, tabInternals.onClose, false);
  tabInternals.onClose = null;
  rawTabNS(tabInternals.tab).tab = null;
  tabInternals.tab = null;
  tabInternals.window = null;
}

function onTabReady(event) {
  let win = event.target.defaultView;

  // ignore frames
  if (win === win.top) {
    emit(this, 'ready', this);
  }
}

function onTabLoad (event) {
  let win = event.target.defaultView;

  // ignore frames
  if (win === win.top) {
    emit(this, 'load', this);
  }
}

function onTabPageShow(event) {
  let win = event.target.defaultView;
  if (win === win.top)
    emit(this, 'pageshow', this, event.persisted);
}

// TabClose
function onTabClose(event) {
  let rawTab = getTabForBrowser(event.target);
  if (tabNS(this).tab !== rawTab)
    return;

  emit(this, EVENTS.close.name, this);
  cleanupTab(this);
};

isPrivate.implement(Tab, tab => {
  return isWindowPrivate(getTabContentWindow(tabNS(tab).tab));
});

// Implement `modelFor` function for the Tab instances.
modelFor.when(isTab, rawTab => {
  return rawTabNS(rawTab).tab;
});
