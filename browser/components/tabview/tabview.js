/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource:///modules/tabview/utils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "tabviewBundle", function() {
  return Services.strings.
    createBundle("chrome://browser/locale/tabview.properties");
});
XPCOMUtils.defineLazyGetter(this, "tabbrowserBundle", function() {
  return Services.strings.
    createBundle("chrome://browser/locale/tabbrowser.properties");
});

function tabviewString(name) tabviewBundle.GetStringFromName('tabview.' + name);
function tabbrowserString(name) tabbrowserBundle.GetStringFromName(name);

XPCOMUtils.defineLazyGetter(this, "gPrefBranch", function() {
  return Services.prefs.getBranch("browser.panorama.");
});

XPCOMUtils.defineLazyServiceGetter(this, "gPrivateBrowsing",
  "@mozilla.org/privatebrowsing;1", "nsIPrivateBrowsingService");

XPCOMUtils.defineLazyModuleGetter(this, "gPageThumbnails",
  "resource:///modules/PageThumbs.jsm", "PageThumbs");

var gWindow = window.parent;
var gBrowser = gWindow.gBrowser;
var gTabView = gWindow.TabView;
var gTabViewDeck = gWindow.document.getElementById("tab-view-deck");
var gBrowserPanel = gWindow.document.getElementById("browser-panel");
var gTabViewFrame = gWindow.document.getElementById("tab-view");

let AllTabs = {
  _events: {
    attrModified: "TabAttrModified",
    close:        "TabClose",
    move:         "TabMove",
    open:         "TabOpen",
    select:       "TabSelect",
    pinned:       "TabPinned",
    unpinned:     "TabUnpinned"
  },

  get tabs() {
    return Array.filter(gBrowser.tabs, function (tab) Utils.isValidXULTab(tab));
  },

  register: function AllTabs_register(eventName, callback) {
    gBrowser.tabContainer.addEventListener(this._events[eventName], callback, false);
  },

  unregister: function AllTabs_unregister(eventName, callback) {
    gBrowser.tabContainer.removeEventListener(this._events[eventName], callback, false);
  }
};

# NB: Certain files need to evaluate before others

#include iq.js
#include storage.js
#include items.js
#include groupitems.js
#include tabitems.js
#include favicons.js
#include drag.js
#include trench.js
#include search.js
#include telemetry.js
#include ui.js
