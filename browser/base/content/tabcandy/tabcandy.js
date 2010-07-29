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
  return gWindow.document.getElementById("tab-candy-deck");
});

XPCOMUtils.defineLazyGetter(this, "gTabViewFrame", function() {
  return gWindow.document.getElementById("tab-candy");
});

# NB: Certain files need to evaluate before others

#include core/profile.js
#include core/iq.js
#include app/storage.js
#include app/items.js
#include app/groups.js
#include app/tabitems.js

#include app/drag.js
#include app/trench.js
#include app/infoitems.js
#include app/ui.js
