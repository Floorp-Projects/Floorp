const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource:///modules/tabview/AllTabs.jsm");
Cu.import("resource:///modules/tabview/groups.jsm");
Cu.import("resource:///modules/tabview/utils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "gWindow", function() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIWebNavigation).
    QueryInterface(Ci.nsIDocShell).
    chromeEventHandler.ownerDocument.defaultView;
});

XPCOMUtils.defineLazyGetter(this, "gBrowser", function() gWindow.gBrowser);

XPCOMUtils.defineLazyGetter(this, "gTabViewDeck", function() {
  return gWindow.document.getElementById("tab-view-deck");
});

XPCOMUtils.defineLazyGetter(this, "gTabViewFrame", function() {
  return gWindow.document.getElementById("tab-view");
});

XPCOMUtils.defineLazyGetter(this, "tabviewBundle", function() {
  return Services.strings.
    createBundle("chrome://browser/locale/tabview.properties");
});

function tabviewString(name) tabviewBundle.GetStringFromName('tabview.' + name);

XPCOMUtils.defineLazyGetter(this, "gPrefBranch", function() {
  return Cc["@mozilla.org/preferences-service;1"].
    getService(Ci.nsIPrefService).
    getBranch("browser.panorama.");
});

XPCOMUtils.defineLazyGetter(this, "gPrivateBrowsing", function() {
  return Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
});

# NB: Certain files need to evaluate before others

#include iq.js
#include storage.js
#include items.js
#include groupitems.js
#include tabitems.js
#include drag.js
#include trench.js
#include ui.js
#include search.js
