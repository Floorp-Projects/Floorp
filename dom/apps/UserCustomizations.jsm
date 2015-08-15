/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["UserCustomizations"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Extension.jsm");


XPCOMUtils.defineLazyServiceGetter(this, "console",
                                   "@mozilla.org/consoleservice;1",
                                   "nsIConsoleService");

function debug(aMsg) {
  if (!UserCustomizations._debug) {
    return;
  }
  dump("-*-*- UserCustomizations " + aMsg + "\n");
}

function log(aStr) {
  console.logStringMessage(aStr);
}

this.UserCustomizations = {
  extensions: new Map(), // id -> extension. Needed to disable extensions.

  register: function(aApp) {
    if (!this._enabled || !aApp.enabled || aApp.role != "addon") {
      debug("Rejecting registration (global enabled=" + this._enabled +
            ") (app role=" + aApp.role +
            ", enabled=" + aApp.enabled + ")");
      return;
    }

    debug("Starting customization registration for " + aApp.manifestURL + "\n");

    let extension = new Extension({
      id: aApp.manifestURL,
      resourceURI: Services.io.newURI(aApp.origin + "/", null, null)
    });

    this.extensions.set(aApp.manifestURL, extension);
    extension.startup();
  },

  unregister: function(aApp) {
    if (!this._enabled) {
      return;
    }

    debug("Starting customization unregistration for " + aApp.manifestURL);
    if (this.extensions.has(aApp.manifestURL)) {
      this.extensions.get(aApp.manifestURL).shutdown();
      this.extensions.delete(aApp.manifestURL);
    }
  },

  init: function() {
    this._enabled = false;
    try {
      this._enabled = Services.prefs.getBoolPref("dom.apps.customization.enabled");
    } catch(e) {}
  },
}

UserCustomizations.init();
