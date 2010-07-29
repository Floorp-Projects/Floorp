// profile.js starts a timer to see how long this file takes to load, so it needs to be first.
// The file should be removed before we ship. 
#include profile.js

Components.utils.import("resource://gre/modules/tabview/tabs.js");
Components.utils.import("resource://gre/modules/tabview/utils.js");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "gWindow", function() {
  return window.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
    getInterface(Components.interfaces.nsIWebNavigation).
    QueryInterface(Components.interfaces.nsIDocShell).
    chromeEventHandler.ownerDocument.defaultView;
});

XPCOMUtils.defineLazyGetter(this, "gBrowser", function() gWindow.gBrowser);

XPCOMUtils.defineLazyGetter(this, "gTabViewDeck", function() {
  return gWindow.document.getElementById("tab-view-deck");
});

XPCOMUtils.defineLazyGetter(this, "gTabViewFrame", function() {
  return gWindow.document.getElementById("tab-view");
});

# NB: Certain files need to evaluate before others

#include iq.js
#include storage.js
#include items.js
#include groups.js
#include tabitems.js
#include drag.js
#include trench.js
#include infoitems.js
#include ui.js
