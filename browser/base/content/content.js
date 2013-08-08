/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this,
  "LoginManagerContent", "resource://gre/modules/LoginManagerContent.jsm");
XPCOMUtils.defineLazyModuleGetter(this,
  "InsecurePasswordUtils", "resource://gre/modules/InsecurePasswordUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

// Bug 671101 - directly using webNavigation in this context
// causes docshells to leak
this.__defineGetter__("webNavigation", function () {
  return docShell.QueryInterface(Ci.nsIWebNavigation);
});

addMessageListener("WebNavigation:LoadURI", function (message) {
  let flags = message.json.flags || webNavigation.LOAD_FLAGS_NONE;

  webNavigation.loadURI(message.json.uri, flags, null, null, null);
});

addMessageListener("Browser:HideSessionRestoreButton", function (message) {
  // Hide session restore button on about:home
  let doc = content.document;
  let container;
  if (doc.documentURI.toLowerCase() == "about:home" &&
      (container = doc.getElementById("sessionRestoreContainer"))){
    container.hidden = true;
  }
});

if (Services.prefs.getBoolPref("browser.tabs.remote")) {
  addEventListener("contextmenu", function (event) {
    sendAsyncMessage("contextmenu", {}, { event: event });
  }, false);
} else {
  addEventListener("DOMContentLoaded", function(event) {
    LoginManagerContent.onContentLoaded(event);
  });
  addEventListener("DOMFormHasPassword", function(event) {
    InsecurePasswordUtils.checkForInsecurePasswords(event.target);
  });
  addEventListener("DOMAutoComplete", function(event) {
    LoginManagerContent.onUsernameInput(event);
  });
  addEventListener("blur", function(event) {
    LoginManagerContent.onUsernameInput(event);
  });
}

let AboutHomeListener = {
  init: function() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);

    addMessageListener("AboutHome:Update", this);
  },

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
    case "AboutHome:Update":
      this.onUpdate(aMessage.data);
      break;
    }
  },

  onUpdate: function(aData) {
    let doc = content.document;
    if (doc.documentURI.toLowerCase() != "about:home")
      return;

    if (aData.showRestoreLastSession && !PrivateBrowsingUtils.isWindowPrivate(content))
      doc.getElementById("launcher").setAttribute("session", "true");

    // Inject search engine and snippets URL.
    let docElt = doc.documentElement;
    // set the following attributes BEFORE searchEngineURL, which triggers to
    // show the snippets when it's set.
    docElt.setAttribute("snippetsURL", aData.snippetsURL);
    if (aData.showKnowYourRights)
      docElt.setAttribute("showKnowYourRights", "true");
    docElt.setAttribute("snippetsVersion", aData.snippetsVersion);

    let engine = aData.defaultSearchEngine;
    docElt.setAttribute("searchEngineName", engine.name);
    docElt.setAttribute("searchEnginePostData", engine.postDataString || "");
    // Again, keep the searchEngineURL as the last attribute, because the
    // mutation observer in aboutHome.js is counting on that.
    docElt.setAttribute("searchEngineURL", engine.searchURL);
  },

  onPageLoad: function(aDocument) {
    // XXX bug 738646 - when Marketplace is launched, remove this statement and
    // the hidden attribute set on the apps button in aboutHome.xhtml
    if (Services.prefs.getPrefType("browser.aboutHome.apps") == Services.prefs.PREF_BOOL &&
        Services.prefs.getBoolPref("browser.aboutHome.apps"))
      doc.getElementById("apps").removeAttribute("hidden");

    sendAsyncMessage("AboutHome:RequestUpdate");

    aDocument.addEventListener("AboutHomeSearchEvent", function onSearch(e) {
      sendAsyncMessage("AboutHome:Search", { engineName: e.detail });
    }, true, true);
  },

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    let doc = aWebProgress.DOMWindow.document;
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW &&
        Components.isSuccessCode(aStatus) &&
        doc.documentURI.toLowerCase() == "about:home" &&
        !doc.documentElement.hasAttribute("hasBrowserHandlers")) {
      // STATE_STOP may be received twice for documents, thus store an
      // attribute to ensure handling it just once.
      doc.documentElement.setAttribute("hasBrowserHandlers", "true");
      addEventListener("click", this.onClick, true);
      addEventListener("pagehide", function onPageHide(event) {
        if (event.target.defaultView.frameElement)
          return;
        removeEventListener("click", this.onClick, true);
        removeEventListener("pagehide", onPageHide, true);
        if (event.target.documentElement)
          event.target.documentElement.removeAttribute("hasBrowserHandlers");
      }, true);

      // We also want to make changes to page UI for unprivileged about pages.
      this.onPageLoad(doc);
    }
  },

  onClick: function(aEvent) {
    if (!aEvent.isTrusted || // Don't trust synthetic events
        aEvent.button == 2 || aEvent.target.localName != "button") {
      return;
    }

    let originalTarget = aEvent.originalTarget;
    let ownerDoc = originalTarget.ownerDocument;
    let elmId = originalTarget.getAttribute("id");

    switch (elmId) {
      case "restorePreviousSession":
        sendAsyncMessage("AboutHome:RestorePreviousSession");
        ownerDoc.getElementById("launcher").removeAttribute("session");
        break;

      case "downloads":
        sendAsyncMessage("AboutHome:Downloads");
        break;

      case "bookmarks":
        sendAsyncMessage("AboutHome:Bookmarks");
        break;

      case "history":
        sendAsyncMessage("AboutHome:History");
        break;

      case "apps":
        sendAsyncMessage("AboutHome:Apps");
        break;

      case "addons":
        sendAsyncMessage("AboutHome:Addons");
        break;

      case "sync":
        sendAsyncMessage("AboutHome:Sync");
        break;

      case "settings":
        sendAsyncMessage("AboutHome:Settings");
        break;
    }
  },

  QueryInterface: function QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};
AboutHomeListener.init();
