/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script contains code that requires a tab browser. */

/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");

ChromeUtils.import("resource://gre/modules/ActorManagerChild.jsm");

ActorManagerChild.attach(this, "browsers");

// TabChildGlobal
var global = this;

XPCOMUtils.defineLazyProxy(this, "LightweightThemeChildHelper",
  "resource:///modules/LightweightThemeChildHelper.jsm");

let themeablePagesWhitelist = new Set([
  "about:home",
  "about:newtab",
  "about:welcome",
]);

addEventListener("pageshow", function({ originalTarget }) {
  if (originalTarget.defaultView == content && themeablePagesWhitelist.has(content.document.documentURI)) {
    LightweightThemeChildHelper.listen(themeablePagesWhitelist);
    LightweightThemeChildHelper.update(chromeOuterWindowID, content);
  }
}, false, true);

// Keep a reference to the translation content handler to avoid it it being GC'ed.
var trHandler = null;
if (Services.prefs.getBoolPref("browser.translation.detectLanguage")) {
  ChromeUtils.import("resource:///modules/translation/TranslationContentHandler.jsm");
  trHandler = new TranslationContentHandler(global, docShell);
}

var WebBrowserChrome = {
  onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab) {
    return BrowserUtils.onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab);
  },

  // Check whether this URI should load in the current process
  shouldLoadURI(aDocShell, aURI, aReferrer, aHasPostData, aTriggeringPrincipal) {
    if (!E10SUtils.shouldLoadURI(aDocShell, aURI, aReferrer, aHasPostData)) {
      E10SUtils.redirectLoad(aDocShell, aURI, aReferrer, aTriggeringPrincipal, false);
      return false;
    }

    return true;
  },

  shouldLoadURIInThisProcess(aURI) {
    return E10SUtils.shouldLoadURIInThisProcess(aURI);
  },

  // Try to reload the currently active or currently loading page in a new process.
  reloadInFreshProcess(aDocShell, aURI, aReferrer, aTriggeringPrincipal, aLoadFlags) {
    E10SUtils.redirectLoad(aDocShell, aURI, aReferrer, aTriggeringPrincipal, true, aLoadFlags);
    return true;
  }
};

if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
  let tabchild = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsITabChild);
  tabchild.webBrowserChrome = WebBrowserChrome;
}

Services.obs.notifyObservers(this, "tab-content-frameloader-created");

// Remove this once bug 1397365 is fixed.
addEventListener("MozAfterPaint", function onFirstNonBlankPaint() {
  if (content.document.documentURI == "about:blank" && !content.opener)
    return;
  removeEventListener("MozAfterPaint", onFirstNonBlankPaint);
  sendAsyncMessage("Browser:FirstNonBlankPaint");
});
