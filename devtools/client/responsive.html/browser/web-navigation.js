/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cu, Cr } = require("chrome");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
const Services = require("Services");
const { NetUtil } = require("resource://gre/modules/NetUtil.jsm");
const { Utils } = require("resource://gre/modules/sessionstore/Utils.jsm");

function readInputStreamToString(stream) {
  return NetUtil.readInputStreamToString(stream, stream.available());
}

/**
 * This object aims to provide the nsIWebNavigation interface for mozbrowser elements.
 * nsIWebNavigation is one of the interfaces expected on <xul:browser>s, so this wrapper
 * helps mozbrowser elements support this.
 *
 * It attempts to use the mozbrowser API wherever possible, however some methods don't
 * exist yet, so we fallback to the WebNavigation frame script messages in those cases.
 * Ideally the mozbrowser API would eventually be extended to cover all properties and
 * methods used here.
 *
 * This is largely copied from RemoteWebNavigation.js, which uses the message manager to
 * perform all actions.
 */
function BrowserElementWebNavigation(browser) {
  this._browser = browser;
}

BrowserElementWebNavigation.prototype = {

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIWebNavigation
  ]),

  get _mm() {
    return this._browser.frameLoader.messageManager;
  },

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
    this.loadURIWithOptions(uri, flags, referrer,
                            Ci.nsIHttpChannel.REFERRER_POLICY_UNSET,
                            postData, headers, null, null);
  },

  loadURIWithOptions(uri, flags, referrer, referrerPolicy, postData, headers,
                     baseURI, triggeringPrincipal) {
    // No equivalent in the current BrowserElement API
    this._sendMessage("WebNavigation:LoadURI", {
      uri,
      flags,
      referrer: referrer ? referrer.spec : null,
      referrerPolicy: referrerPolicy,
      postData: postData ? readInputStreamToString(postData) : null,
      headers: headers ? readInputStreamToString(headers) : null,
      baseURI: baseURI ? baseURI.spec : null,
      triggeringPrincipal: triggeringPrincipal
                           ? Utils.serializePrincipal(triggeringPrincipal)
                           : null,
      requestTime: Services.telemetry.msSystemNow(),
    });
  },

  setOriginAttributesBeforeLoading(originAttributes) {
    // No equivalent in the current BrowserElement API
    this._sendMessage("WebNavigation:SetOriginAttributes", {
      originAttributes,
    });
  },

  reload(flags) {
    let hardReload = false;
    if (flags & this.LOAD_FLAGS_BYPASS_PROXY ||
        flags & this.LOAD_FLAGS_BYPASS_CACHE) {
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
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  set sessionHistory(value) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  _sendMessage(message, data) {
    try {
      this._mm.sendAsyncMessage(message, data);
    } catch (e) {
      Cu.reportError(e);
    }
  },

  swapBrowser(browser) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  copyStateFrom(otherWebNavigation) {
    const state = [
      "canGoBack",
      "canGoForward",
      "_currentURI",
    ];
    for (let property of state) {
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

for (let flag of FLAGS) {
  BrowserElementWebNavigation.prototype[flag] = Ci.nsIWebNavigation[flag];
}

exports.BrowserElementWebNavigation = BrowserElementWebNavigation;
