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

function tabviewString(name) tabviewBundle.GetStringFromName('tabview.' + name);

XPCOMUtils.defineLazyGetter(this, "gPrefBranch", function() {
  return Services.prefs.getBranch("browser.panorama.");
});

XPCOMUtils.defineLazyGetter(this, "gPrivateBrowsing", function() {
  return Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
});

XPCOMUtils.defineLazyGetter(this, "gFavIconService", function() {
  return Cc["@mozilla.org/browser/favicon-service;1"].
           getService(Ci.nsIFaviconService);
});

XPCOMUtils.defineLazyGetter(this, "gNetUtil", function() {
  var obj = {};
  Cu.import("resource://gre/modules/NetUtil.jsm", obj);
  return obj.NetUtil;
});

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
    return Array.filter(gBrowser.tabs, function (tab) !tab.closing);
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
#include drag.js
#include trench.js
#include thumbnailStorage.js
#include ui.js
#include search.js
