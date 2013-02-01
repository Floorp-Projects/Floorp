/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'unstable'
};

const { defer } = require("../lang/functional");
const { windows, isBrowser } = require('../window/utils');
const { Ci } = require('chrome');

function activateTab(tab, window) {
  let gBrowser = getTabBrowserForTab(tab);

  // normal case
  if (gBrowser) {
    gBrowser.selectedTab = tab;
  }
  // fennec ?
  else if (window && window.BrowserApp) {
    window.BrowserApp.selectTab(tab);
  }
  return null;
}
exports.activateTab = activateTab;

function getTabBrowser(window) {
  return window.gBrowser;
}
exports.getTabBrowser = getTabBrowser;

function getTabContainer(window) {
  return getTabBrowser(window).tabContainer;
}
exports.getTabContainer = getTabContainer;

/**
 * Returns the tabs for the `window` if given, or the tabs
 * across all the browser's windows otherwise.
 *
 * @param {nsIWindow} [window]
 *    A reference to a window
 *
 * @returns {Array} an array of Tab objects
 */
function getTabs(window) {
  if (arguments.length === 0) {
    return windows().filter(isBrowser).reduce(function(tabs, window) {
      return tabs.concat(getTabs(window))
    }, []);
  }

  // fennec
  if (window.BrowserApp)
    return window.BrowserApp.tabs;

  // firefox - default
  return Array.slice(getTabContainer(window).children);
}
exports.getTabs = getTabs;

function getActiveTab(window) {
  return window.gBrowser.selectedTab;
}
exports.getActiveTab = getActiveTab;

function getOwnerWindow(tab) {
  // normal case
  if (tab.ownerDocument)
    return tab.ownerDocument.defaultView;

  // try fennec case
  return getWindowHoldingTab(tab);
}
exports.getOwnerWindow = getOwnerWindow;

// fennec
function getWindowHoldingTab(rawTab) {
  for each (let window in windows()) {
    // this function may be called when not using fennec
    if (!window.BrowserApp)
      continue;

    for each (let tab in window.BrowserApp.tabs) {
      if (tab === rawTab)
        return window;
    }
  }

  return null;
}

function openTab(window, url, options) {
  options = options || {};

  // fennec?
  if (window.BrowserApp) {
    return window.BrowserApp.addTab(url, {
      selected: options.inBackground ? false : true,
      pinned: options.isPinned || false
    });
  }
  return window.gBrowser.addTab(url);
};
exports.openTab = openTab;

function isTabOpen(tab) {
  // try normal case then fennec case
  return !!((tab.linkedBrowser) || getWindowHoldingTab(tab));
}
exports.isTabOpen = isTabOpen;

function closeTab(tab) {
  let gBrowser = getTabBrowserForTab(tab);
  // normal case?
  if (gBrowser)
    return gBrowser.removeTab(tab);

  let window = getWindowHoldingTab(tab);
  // fennec?
  if (window && window.BrowserApp)
    return window.BrowserApp.closeTab(tab);
  return null;
}
exports.closeTab = closeTab;

function getURI(tab) {
  if (tab.browser) // fennec
    return tab.browser.currentURI.spec;
  return tab.linkedBrowser.currentURI.spec;
}
exports.getURI = getURI;

function getTabBrowserForTab(tab) {
  let outerWin = getOwnerWindow(tab);
  if (outerWin)
    return getOwnerWindow(tab).gBrowser;
  return null;
}
exports.getTabBrowserForTab = getTabBrowserForTab;

function getBrowserForTab(tab) {
  if (tab.browser) // fennec
    return tab.browser;

  return tab.linkedBrowser;
}
exports.getBrowserForTab = getBrowserForTab;

function getTabId(tab) {
  if (tab.browser) // fennec
    return tab.id

  return String.split(tab.linkedPanel, 'panel').pop();
}
exports.getTabId = getTabId;


function getTabTitle(tab) {
  return getBrowserForTab(tab).contentDocument.title || tab.label || "";
}
exports.getTabTitle = getTabTitle;

function setTabTitle(tab, title) {
  title = String(title);
  if (tab.browser)
    tab.browser.contentDocument.title = title;
  tab.label = String(title);
}
exports.setTabTitle = setTabTitle;

function getTabContentWindow(tab) {
  return getBrowserForTab(tab).contentWindow;
}
exports.getTabContentWindow = getTabContentWindow;

/**
 * Returns all tabs' content windows across all the browsers' windows
 */
function getAllTabContentWindows() {
  return getTabs().map(getTabContentWindow);
}
exports.getAllTabContentWindows = getAllTabContentWindows;

function getTabForContentWindow(window) {
  // Retrieve the topmost frame container. It can be either <xul:browser>,
  // <xul:iframe/> or <html:iframe/>. But in our case, it should be xul:browser.
  let browser = window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .QueryInterface(Ci.nsIDocShell)
                   .chromeEventHandler;
  // Is null for toplevel documents
  if (!browser)
    return false;
  // Retrieve the owner window, should be browser.xul one
  let chromeWindow = browser.ownerDocument.defaultView;

  // Ensure that it is top-level browser window.
  // We need extra checks because of Mac hidden window that has a broken
  // `gBrowser` global attribute.
  if ('gBrowser' in chromeWindow && chromeWindow.gBrowser &&
      'browsers' in chromeWindow.gBrowser) {
    // Looks like we are on Firefox Desktop
    // Then search for the position in tabbrowser in order to get the tab object
    let browsers = chromeWindow.gBrowser.browsers;
    let i = browsers.indexOf(browser);
    if (i !== -1)
      return chromeWindow.gBrowser.tabs[i];
    return null;
  }
  else if ('BrowserApp' in chromeWindow) {
    // Looks like we are on Firefox Mobile
    return chromeWindow.BrowserApp.getTabForWindow(window)
  }

  return null;
}
exports.getTabForContentWindow = getTabForContentWindow;

function getTabURL(tab) {
  if (tab.browser) // fennec
    return String(tab.browser.currentURI.spec);
  return String(getBrowserForTab(tab).currentURI.spec);
}
exports.getTabURL = getTabURL;

function setTabURL(tab, url) {
  url = String(url);
  if (tab.browser)
    return tab.browser.loadURI(url);
  return getBrowserForTab(tab).loadURI(url);
}
// "TabOpen" event is fired when it's still "about:blank" is loaded in the
// changing `location` property of the `contentDocument` has no effect since
// seems to be either ignored or overridden by internal listener, there for
// location change is enqueued for the next turn of event loop.
exports.setTabURL = defer(setTabURL);

function getTabContentType(tab) {
  return getBrowserForTab(tab).contentDocument.contentType;
}
exports.getTabContentType = getTabContentType;

function getSelectedTab(window) {
  if (window.BrowserApp) // fennec?
    return window.BrowserApp.selectedTab;
  if (window.gBrowser)
    return window.gBrowser.selectedTab;
  return null;
}
exports.getSelectedTab = getSelectedTab;
