/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutCompat"];

const Services =
  globalThis.Services ||
  ChromeUtils.import("resource://gre/modules/Services.jsm").Services;

const addonID = "webcompat@mozilla.org";
const addonPageRelativeURL = "/about-compat/aboutCompat.html";

function AboutCompat() {
  this.chromeURL = WebExtensionPolicy.getByID(addonID).getURL(
    addonPageRelativeURL
  );
}
AboutCompat.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIAboutModule"]),
  getURIFlags() {
    return (
      Ci.nsIAboutModule.URI_MUST_LOAD_IN_EXTENSION_PROCESS |
      Ci.nsIAboutModule.IS_SECURE_CHROME_UI
    );
  },

  newChannel(aURI, aLoadInfo) {
    const uri = Services.io.newURI(this.chromeURL);
    const channel = Services.io.newChannelFromURIWithLoadInfo(uri, aLoadInfo);
    channel.originalURI = aURI;

    channel.owner = (
      Services.scriptSecurityManager.createContentPrincipal ||
      Services.scriptSecurityManager.createCodebasePrincipal
    )(uri, aLoadInfo.originAttributes);
    return channel;
  },
};
