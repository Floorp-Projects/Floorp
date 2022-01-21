/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, Services, XPCOMUtils */

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "resProto",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsISubstitutingProtocolHandler"
);

const ResourceSubstitution = "webcompat";
const ProcessScriptURL = "resource://webcompat/aboutPageProcessScript.js";
const ContractID = "@mozilla.org/network/protocol/about;1?what=compat";

this.aboutPage = class extends ExtensionAPI {
  onStartup() {
    const { rootURI } = this.extension;

    resProto.setSubstitution(
      ResourceSubstitution,
      Services.io.newURI("about-compat/", null, rootURI)
    );

    if (!(ContractID in Cc)) {
      Services.ppmm.loadProcessScript(ProcessScriptURL, true);
      this.processScriptRegistered = true;
    }
  }

  onShutdown() {
    resProto.setSubstitution(ResourceSubstitution, null);

    if (this.processScriptRegistered) {
      Services.ppmm.removeDelayedProcessScript(ProcessScriptURL);
    }
  }
};
