/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Class } = require('../core/heritage');
const { Tab } = require('../tabs/tab');
const { browserWindows } = require('./fennec');
const { windowNS } = require('../window/namespace');
const { tabsNS, tabNS } = require('../tabs/namespace');
const { openTab, getTabs, getSelectedTab, getTabForBrowser: getRawTabForBrowser,
        getTabContentWindow } = require('../tabs/utils');
const { Options } = require('../tabs/common');
const { getTabForBrowser, getTabForRawTab } = require('../tabs/helpers');
const { on, once, off, emit } = require('../event/core');
const { method } = require('../lang/functional');
const { EVENTS } = require('../tabs/events');
const { EventTarget } = require('../event/target');
const { when: unload } = require('../system/unload');
const { windowIterator } = require('../deprecated/window-utils');
const { List, addListItem, removeListItem } = require('../util/list');
const { isPrivateBrowsingSupported } = require('../self');
const { isTabPBSupported, ignoreWindow } = require('../private-browsing/utils');

const mainWindow = windowNS(browserWindows.activeWindow).window;

const ERR_FENNEC_MSG = 'This method is not yet supported by Fennec';

const supportPrivateTabs = isPrivateBrowsingSupported && isTabPBSupported;

const Tabs = Class({
  implements: [ List ],
  extends: EventTarget,
  initialize: function initialize(options) {
    let tabsInternals = tabsNS(this);
    let window = tabsNS(this).window = options.window || mainWindow;

    EventTarget.prototype.initialize.call(this, options);
    List.prototype.initialize.apply(this, getTabs(window).map(Tab));

    // TabOpen event
    window.BrowserApp.deck.addEventListener(EVENTS.open.dom, onTabOpen, false);

    // TabSelect
    window.BrowserApp.deck.addEventListener(EVENTS.activate.dom, onTabSelect, false);
  },
  get activeTab() {
    return getTabForRawTab(getSelectedTab(tabsNS(this).window));
  },
  open: function(options) {
    options = Options(options);
    let activeWin = browserWindows.activeWindow;

    if (options.isPinned) {
      console.error(ERR_FENNEC_MSG); // TODO
    }

    let rawTab = openTab(windowNS(activeWin).window, options.url, {
      inBackground: options.inBackground,
      isPrivate: supportPrivateTabs && options.isPrivate
    });

    // by now the tab has been created
    let tab = getTabForRawTab(rawTab);

    if (options.onClose)
      tab.on('close', options.onClose);

    if (options.onOpen) {
      // NOTE: on Fennec this will be true
      if (tabNS(tab).opened)
        options.onOpen(tab);

      tab.on('open', options.onOpen);
    }

    if (options.onReady)
      tab.on('ready', options.onReady);

    if (options.onPageShow)
      tab.on('pageshow', options.onPageShow);

    if (options.onActivate)
      tab.on('activate', options.onActivate);

    return tab;
  }
});
let gTabs = exports.tabs = Tabs(mainWindow);

function tabsUnloader(event, window) {
  window = window || (event && event.target);
  if (!(window && window.BrowserApp))
    return;
  window.BrowserApp.deck.removeEventListener(EVENTS.open.dom, onTabOpen, false);
  window.BrowserApp.deck.removeEventListener(EVENTS.activate.dom, onTabSelect, false);
}

// unload handler
unload(function() {
  for (let window in windowIterator()) {
    tabsUnloader(null, window);
  }
});

function addTab(tab) {
  addListItem(gTabs, tab);
  return tab;
}

function removeTab(tab) {
  removeListItem(gTabs, tab);
  return tab;
}

// TabOpen
function onTabOpen(event) {
  let browser = event.target;

  // Eventually ignore private tabs
  if (ignoreWindow(browser.contentWindow))
    return;

  let tab = getTabForBrowser(browser);
  if (tab === null) {
    let rawTab = getRawTabForBrowser(browser);

    // create a Tab instance for this new tab
    tab = addTab(Tab(rawTab));
  }

  tabNS(tab).opened = true;

  tab.on('ready', function() emit(gTabs, 'ready', tab));
  tab.once('close', onTabClose);

  tab.on('pageshow', function(_tab, persisted)
    emit(gTabs, 'pageshow', tab, persisted));
  
  emit(tab, 'open', tab);
  emit(gTabs, 'open', tab);
}

// TabSelect
function onTabSelect(event) {
  let browser = event.target;

  // Eventually ignore private tabs
  if (ignoreWindow(browser.contentWindow))
    return;

  // Set value whenever new tab becomes active.
  let tab = getTabForBrowser(browser);
  emit(tab, 'activate', tab);
  emit(gTabs, 'activate', tab);

  for each (let t in gTabs) {
    if (t === tab) continue;
    emit(t, 'deactivate', t);
    emit(gTabs, 'deactivate', t);
  }
}

// TabClose
function onTabClose(tab) {
  removeTab(tab);
  emit(gTabs, EVENTS.close.name, tab);
}
