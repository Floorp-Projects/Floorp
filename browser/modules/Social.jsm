/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let EXPORTED_SYMBOLS = ["Social"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SocialService",
  "resource://gre/modules/SocialService.jsm");

let Social = {
  provider: null,
  init: function Social_init(callback) {
    if (this.provider) {
      schedule(callback);
      return;
    }

    // Eventually this might want to retrieve a specific provider, but for now
    // just use the first available.
    SocialService.getProviderList(function (providers) {
      if (providers.length)
        this.provider = providers[0];
      callback();
      Services.obs.notifyObservers(null, "test-social-ui-ready", "");
    }.bind(this));
  },

  get enabled() {
    return SocialService.enabled;
  },

  get uiVisible() {
    return this.provider && this.provider.enabled && this.provider.port;
  },

  sendWorkerMessage: function Social_sendWorkerMessage(message) {
    // Responses aren't handled yet because there is no actions to perform
    // based on the response from the provider at this point.
    if (this.provider && this.provider.port)
      this.provider.port.postMessage(message);
  },

  // Sharing functionality
  _getShareablePageUrl: function Social_getShareablePageUrl(aURI) {
    let uri = aURI.clone();
    try {
      // Setting userPass on about:config throws.
      uri.userPass = "";
    } catch (e) {}
    return uri.spec;
  },

  isPageShared: function Social_isPageShared(aURI) {
    let url = this._getShareablePageUrl(aURI);
    return this._sharedUrls.hasOwnProperty(url);
  },

  sharePage: function Social_sharePage(aURI) {
    let url = this._getShareablePageUrl(aURI);
    this._sharedUrls[url] = true;
    this.sendWorkerMessage({
      topic: "social.user-recommend",
      data: { url: url }
    });
  },

  unsharePage: function Social_unsharePage(aURI) {
    let url = this._getShareablePageUrl(aURI);
    delete this._sharedUrls[url];
    this.sendWorkerMessage({
      topic: "social.user-unrecommend",
      data: { url: url }
    });
  },

  _sharedUrls: {}
};

function schedule(callback) {
  Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
}
