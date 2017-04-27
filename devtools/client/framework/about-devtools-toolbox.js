/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Register about:devtools-toolbox which allows to open a devtools toolbox
// in a Firefox tab or a custom html iframe in browser.html

const { Ci, Cu, Cm, components } = require("chrome");
const registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
const Services = require("Services");
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
const { nsIAboutModule } = Ci;

function AboutURL() {}

AboutURL.prototype = {
  uri: Services.io.newURI("chrome://devtools/content/framework/toolbox.xul"),
  classDescription: "about:devtools-toolbox",
  classID: components.ID("11342911-3135-45a8-8d71-737a2b0ad469"),
  contractID: "@mozilla.org/network/protocol/about;1?what=devtools-toolbox",

  QueryInterface: XPCOMUtils.generateQI([nsIAboutModule]),

  newChannel: function (aURI, aLoadInfo) {
    let chan = Services.io.newChannelFromURIWithLoadInfo(this.uri, aLoadInfo);
    chan.owner = Services.scriptSecurityManager.getSystemPrincipal();
    return chan;
  },

  getURIFlags: function (aURI) {
    return nsIAboutModule.ALLOW_SCRIPT || nsIAboutModule.ENABLE_INDEXED_DB;
  }
};

AboutURL.createInstance = function (outer, iid) {
  if (outer) {
    throw Cr.NS_ERROR_NO_AGGREGATION;
  }
  return new AboutURL();
};

exports.register = function () {
  if (registrar.isCIDRegistered(AboutURL.prototype.classID)) {
    console.error("Trying to register " + AboutURL.prototype.classDescription +
                  " more than once.");
  } else {
    registrar.registerFactory(AboutURL.prototype.classID,
                       AboutURL.prototype.classDescription,
                       AboutURL.prototype.contractID,
                       AboutURL);
  }
};

exports.unregister = function () {
  if (registrar.isCIDRegistered(AboutURL.prototype.classID)) {
    registrar.unregisterFactory(AboutURL.prototype.classID, AboutURL);
  }
};
