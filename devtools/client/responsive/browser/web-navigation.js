/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu, Cr, components: Components } = require("chrome");
const ChromeUtils = require("ChromeUtils");
const Services = require("Services");

/**
 * This object aims to provide the nsIWebNavigation interface for mozbrowser elements.
 * nsIWebNavigation is one of the interfaces expected on <xul:browser>s, so this wrapper
 * helps mozbrowser elements support this.
 *
 * It attempts to use the mozbrowser API wherever possible, however some methods don't
 * exist yet, so we fallback to the WebNavigation actor in those cases.
 * Ideally the mozbrowser API would eventually be extended to cover all properties and
 * methods used here.
 *
 * This is largely copied from RemoteWebNavigation.js, which uses the WebNavigation
 * actor to perform all actions.
 */
function BrowserElementWebNavigation(browser) {
  this._browser = browser;
}

BrowserElementWebNavigation.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebNavigation]),

  canGoBack: false,
  canGoForward: false,

  goBack() {
    this._browser.goBack();
  },

  goForward() {
    this._browser.goForward();
  },

  gotoIndex(index) {
    // No equivalent in the current BrowserElement API
    this._sendMessage("WebNavigation:GotoIndex", { index });
  },

  loadURI(uri, flags, referrer, postData, headers) {
    // No equivalent in the current BrowserElement API
    this.loadURIWithOptions(
      uri,
      flags,
      referrer,
      Ci.nsIReferrerInfo.EMPTY,
      postData,
      headers,
      null,
      Services.scriptSecurityManager.createNullPrincipal({})
    );
  },

  loadURIWithOptions(
    uri,
    flags,
    referrer,
    referrerPolicy,
    postData,
    headers,
    baseURI,
    triggeringPrincipal
  ) {
    // No equivalent in the current BrowserElement API
    const referrerInfo = Cc["@mozilla.org/referrer-info;1"].createInstance(
      Ci.nsIReferrerInfo
    );
    referrerInfo.init(referrerPolicy, true, referrer);

    this._browser.browsingContext.loadURI(uri, {
      loadFlags: flags,
      referrerInfo,
      postData,
      headers,
      baseURI,
      triggeringPrincipal,
    });
  },

  reload(flags) {
    let hardReload = false;
    if (
      flags & this.LOAD_FLAGS_BYPASS_PROXY ||
      flags & this.LOAD_FLAGS_BYPASS_CACHE
    ) {
      hardReload = true;
    }
    this._browser.reload(hardReload);
  },

  stop(flags) {
    // No equivalent in the current BrowserElement API
    this._sendMessage("WebNavigation:Stop", { flags });
  },

  get document() {
    return this._browser.contentDocument;
  },

  _currentURI: null,
  get currentURI() {
    if (!this._currentURI) {
      this._currentURI = Services.io.newURI("about:blank");
    }
    return this._currentURI;
  },
  set currentURI(uri) {
    this._browser.src = uri.spec;
  },

  referringURI: null,

  // Bug 1233803 - accessing the sessionHistory of remote browsers should be
  // done in content scripts.
  get sessionHistory() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
  set sessionHistory(value) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  _sendMessage(message, data) {
    try {
      if (this._browser.frameLoader) {
        const windowGlobal = this._browser.browsingContext.currentWindowGlobal;
        if (windowGlobal) {
          windowGlobal
            .getActor("WebNavigation")
            .sendAsyncMessage(message, data);
        }
      }
    } catch (e) {
      Cu.reportError(e);
    }
  },

  swapBrowser(browser) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  copyStateFrom(otherWebNavigation) {
    const state = ["canGoBack", "canGoForward", "_currentURI"];
    for (const property of state) {
      this[property] = otherWebNavigation[property];
    }
  },
};

const FLAGS = [
  "LOAD_FLAGS_MASK",
  "LOAD_FLAGS_NONE",
  "LOAD_FLAGS_IS_REFRESH",
  "LOAD_FLAGS_IS_LINK",
  "LOAD_FLAGS_BYPASS_HISTORY",
  "LOAD_FLAGS_REPLACE_HISTORY",
  "LOAD_FLAGS_BYPASS_CACHE",
  "LOAD_FLAGS_BYPASS_PROXY",
  "LOAD_FLAGS_CHARSET_CHANGE",
  "LOAD_FLAGS_STOP_CONTENT",
  "LOAD_FLAGS_FROM_EXTERNAL",
  "LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP",
  "LOAD_FLAGS_FIRST_LOAD",
  "LOAD_FLAGS_ALLOW_POPUPS",
  "LOAD_FLAGS_BYPASS_CLASSIFIER",
  "LOAD_FLAGS_FORCE_ALLOW_COOKIES",
  "STOP_NETWORK",
  "STOP_CONTENT",
  "STOP_ALL",
];

for (const flag of FLAGS) {
  BrowserElementWebNavigation.prototype[flag] = Ci.nsIWebNavigation[flag];
}

exports.BrowserElementWebNavigation = BrowserElementWebNavigation;
