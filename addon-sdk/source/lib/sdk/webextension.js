/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

let webExtension;
let waitForWebExtensionAPI;

module.exports = {
  initFromBootstrapAddonParam(data) {
    if (webExtension) {
      throw new Error("'sdk/webextension' module has been already initialized");
    }

    webExtension = data.webExtension;
  },

  startup() {
    if (!webExtension) {
      return Promise.reject(new Error(
        "'sdk/webextension' module is currently disabled. " +
        "('hasEmbeddedWebExtension' option is missing or set to false)"
      ));
    }

    // NOTE: calling `startup` more than once raises an "Embedded Extension already started"
    // error, but given that SDK addons are going to have access to the startup method through
    // an SDK module that can be required in any part of the addon, it will be nicer if any
    // additional startup calls return the startup promise instead of raising an exception,
    // so that the SDK addon can access the API object in the other addon modules without the
    // need to manually pass this promise around.
    if (!waitForWebExtensionAPI) {
      waitForWebExtensionAPI = webExtension.startup();
    }

    return waitForWebExtensionAPI;
  }
};
