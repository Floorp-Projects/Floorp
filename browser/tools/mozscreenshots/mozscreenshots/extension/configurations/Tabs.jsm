/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Tabs"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const CUST_TAB = "chrome://browser/skin/customize.svg";
const PREFS_TAB = "chrome://browser/skin/settings.svg";
const DEFAULT_FAVICON_TAB = `data:text/html,<meta%20charset="utf-8"><title>No%20favicon</title>`;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://testing-common/TestUtils.jsm");

this.Tabs = {
  init(libDir) {},

  configurations: {
    fiveTabs: {
      selectors: ["#tabbrowser-tabs"],
      async applyConfig() {
        fiveTabsHelper();
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        hoverTab(browserWindow.gBrowser.tabs[3]);
        await new Promise((resolve, reject) => {
          setTimeout(resolve, 3000);
        });
        await allTabTitlesDisplayed(browserWindow);
      },
    },

    fourPinned: {
      selectors: ["#tabbrowser-tabs"],
      async applyConfig() {
        fiveTabsHelper();
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let tab = browserWindow.gBrowser.addTab(PREFS_TAB);
        browserWindow.gBrowser.pinTab(tab);
        tab = browserWindow.gBrowser.addTab(CUST_TAB);
        browserWindow.gBrowser.pinTab(tab);
        tab = browserWindow.gBrowser.addTab("about:privatebrowsing");
        browserWindow.gBrowser.pinTab(tab);
        tab = browserWindow.gBrowser.addTab("about:home");
        browserWindow.gBrowser.pinTab(tab);
        browserWindow.gBrowser.selectTabAtIndex(5);
        hoverTab(browserWindow.gBrowser.tabs[2]);
        // also hover the new tab button
        let newTabButton = browserWindow.document.getAnonymousElementByAttribute(browserWindow.
                           gBrowser.tabContainer, "class", "tabs-newtab-button");
        hoverTab(newTabButton);
        browserWindow.gBrowser.tabs[browserWindow.gBrowser.tabs.length - 1].
                      setAttribute("beforehovered", true);

        await new Promise((resolve, reject) => {
          setTimeout(resolve, 3000);
        });
        await allTabTitlesDisplayed(browserWindow);
      },
    },

    twoPinnedWithOverflow: {
      selectors: ["#tabbrowser-tabs"],
      async applyConfig() {
        fiveTabsHelper();

        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        browserWindow.gBrowser.loadTabs([
          PREFS_TAB,
          CUST_TAB,
          "about:home",
          DEFAULT_FAVICON_TAB,
          "about:newtab",
          "about:addons",
          "about:home",
          DEFAULT_FAVICON_TAB,
          "about:newtab",
          "about:addons",
          "about:home",
          DEFAULT_FAVICON_TAB,
          "about:newtab",
          "about:addons",
          "about:home",
          DEFAULT_FAVICON_TAB,
          "about:newtab",
          "about:addons",
          "about:home",
          DEFAULT_FAVICON_TAB,
          "about:newtab",
          "about:addons",
          "about:home",
          DEFAULT_FAVICON_TAB,
          "about:newtab",
          "about:addons",
          "about:home",
          DEFAULT_FAVICON_TAB,
          "about:newtab",
         ],
         {
           inBackground: true,
           replace: true,
           triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal()
        });
        browserWindow.gBrowser.pinTab(browserWindow.gBrowser.tabs[1]);
        browserWindow.gBrowser.pinTab(browserWindow.gBrowser.tabs[2]);
        browserWindow.gBrowser.selectTabAtIndex(3);
        hoverTab(browserWindow.gBrowser.tabs[5]);

        await new Promise((resolve, reject) => {
          setTimeout(resolve, 3000);
        });
        await allTabTitlesDisplayed(browserWindow);
      },
    },
  },
};


/* helpers */

async function allTabTitlesDisplayed(browserWindow) {
  let specToTitleMap = {
    "about:home": "New Tab",
    "about:newtab": "New Tab",
    "about:addons": "Add-ons Manager",
  };
  specToTitleMap[PREFS_TAB] = "browser/skin/settings.svg";
  specToTitleMap[CUST_TAB] = "browser/skin/customize.svg";
  specToTitleMap[DEFAULT_FAVICON_TAB] = "No favicon";

  let tabTitlePromises = [];
  for (let tab of browserWindow.gBrowser.tabs) {
    function tabTitleLoaded(spec) {
      return () => {
        return spec ? tab.label == specToTitleMap[spec] : false;
      };
    }
    let spec = tab.linkedBrowser &&
               tab.linkedBrowser.documentURI &&
               tab.linkedBrowser.documentURI.spec;
    let promise =
      TestUtils.waitForCondition(tabTitleLoaded(spec), `Tab (${spec}) should be showing "${specToTitleMap[spec]}". Got "${tab.label}"`);
    tabTitlePromises.push(promise);
  }

  return Promise.all(tabTitlePromises);
}

function fiveTabsHelper() {
  // some with no favicon and some with. Selected tab in middle.
  closeAllButOneTab("about:addons");

  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  browserWindow.gBrowser.loadTabs([
    "about:addons",
    "about:home",
    DEFAULT_FAVICON_TAB,
    "about:newtab",
    CUST_TAB,
  ],
  {
    inBackground: true,
    replace: true,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal()
  });
  browserWindow.gBrowser.selectTabAtIndex(1);
}

function closeAllButOneTab(url = "about:blank") {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let gBrowser = browserWindow.gBrowser;
  // Close all tabs except the last so we don't quit the browser.
  while (gBrowser.tabs.length > 1)
    gBrowser.removeTab(gBrowser.selectedTab, {animate: false});
  gBrowser.selectedBrowser.loadURI(url);
  if (gBrowser.selectedTab.pinned)
    gBrowser.unpinTab(gBrowser.selectedTab);
  let newTabButton = browserWindow.document.getAnonymousElementByAttribute(browserWindow.gBrowser.tabContainer, "class", "tabs-newtab-button toolbarbutton-1");
  hoverTab(newTabButton, false);
}

function hoverTab(tab, hover = true) {
  const inIDOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
  if (hover) {
    inIDOMUtils.addPseudoClassLock(tab, ":hover");
  } else {
    inIDOMUtils.clearPseudoClassLocks(tab);
  }
  // XXX TODO: this isn't necessarily testing what we ship
  if (tab.nextElementSibling)
    tab.nextElementSibling.setAttribute("afterhovered", hover || null);
  if (tab.previousElementSibling)
    tab.previousElementSibling.setAttribute("beforehovered", hover || null);
}
