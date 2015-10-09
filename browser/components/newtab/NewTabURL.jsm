/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "NewTabURL" ];

Components.utils.import("resource://gre/modules/Services.jsm");

this.NewTabURL = {
  _url: "about:newtab",
  _remoteUrl: "about:remote-newtab",
  _overridden: false,

  get: function() {
    let output = this._url;
    if (Services.prefs.getBoolPref("browser.newtabpage.remote")) {
      output = this._remoteUrl;
    }
    return output;
  },

  get overridden() {
    return this._overridden;
  },

  override: function(newURL) {
    this._url = newURL;
    this._overridden = true;
    Services.obs.notifyObservers(null, "newtab-url-changed", this._url);
  },

  reset: function() {
    this._url = "about:newtab";
    this._overridden = false;
    Services.obs.notifyObservers(null, "newtab-url-changed", this._url);
  }
};
