/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "NewTabURL" ];

Components.utils.import("resource://gre/modules/Services.jsm");

this.NewTabURL = {
  _url: "about:newtab",
  _overridden: false,

  get: function() {
    return this._url;
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
