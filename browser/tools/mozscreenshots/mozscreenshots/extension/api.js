/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals ExtensionAPI */

XPCOMUtils.defineLazyServiceGetter(
  this,
  "resProto",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsISubstitutingProtocolHandler"
);

this.mozscreenshots = class extends ExtensionAPI {
  async onStartup() {
    let uri = Services.io.newURI("resources/", null, this.extension.rootURI);
    resProto.setSubstitution("mozscreenshots", uri);

    const { TestRunner } = ChromeUtils.importESModule(
      "resource://mozscreenshots/TestRunner.sys.mjs"
    );
    TestRunner.init(this.extension.rootURI);
  }
};
