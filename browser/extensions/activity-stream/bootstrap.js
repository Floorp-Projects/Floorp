/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "resProto",
                                   "@mozilla.org/network/protocol;1?name=resource",
                                   "nsISubstitutingProtocolHandler");

const RESOURCE_HOST = "activity-stream";

// The functions below are required by bootstrap.js

this.install = function install(data, reason) {};

this.startup = function startup(data, reason) {
  resProto.setSubstitutionWithFlags(RESOURCE_HOST,
                                    Services.io.newURI("chrome/content/", null, data.resourceURI),
                                    resProto.ALLOW_CONTENT_ACCESS);
};

this.shutdown = function shutdown(data, reason) {
  resProto.setSubstitution(RESOURCE_HOST, null);
};

this.uninstall = function uninstall(data, reason) {};
