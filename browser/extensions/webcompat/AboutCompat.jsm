/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutCompat"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

const addonID = "webcompat@mozilla.org";
const addonPageRelativeURL = "/aboutCompat.html";

function AboutCompat() {
  this.chromeURL = WebExtensionPolicy.getByID(addonID).getURL(addonPageRelativeURL);
}
AboutCompat.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAboutModule]),
  getURIFlags() {
    return Ci.nsIAboutModule.ALLOW_SCRIPT |
           Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD;
  },

  newChannel(aURI, aLoadInfo) {
    const uri = Services.io.newURI(this.chromeURL);
    aLoadInfo.resultPrincipalURI = uri;
    const channel = Services.io.newChannelFromURIWithLoadInfo(uri, aLoadInfo);
    channel.originalURI = aURI;

    if (this.uriFlags & Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT) {
      channel.owner = null;
    }
    return channel;
  },
};
