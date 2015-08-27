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

XPCOMUtils.defineLazyModuleGetter(this, "ValueExtractor",
  "resource://gre/modules/ValueExtractor.jsm");

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
  appId: new Set(),

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
    extension.startup().then(() => {
      let uri = Services.io.newURI(aApp.origin, null, null);
      this.appId.add(uri.host);
    });
  },

  unregister: function(aApp) {
    if (!this._enabled) {
      return;
    }

    debug("Starting customization unregistration for " + aApp.manifestURL);
    if (this.extensions.has(aApp.manifestURL)) {
      this.extensions.get(aApp.manifestURL).shutdown();
      this.extensions.delete(aApp.manifestURL);
      let uri = Services.io.newURI(aApp.origin, null, null);
      this.appId.delete(uri.host);
    }
  },

  isFromExtension: function(aURI) {
    return this.appId.has(aURI.host);
  },

  // Checks that this is a valid extension manifest.
  // The format is documented at https://developer.chrome.com/extensions/manifest
  checkExtensionManifest: function(aManifest) {
    if (!aManifest) {
      return false;
    }

    const extractor = new ValueExtractor(console);
    const manifestVersionSpec = {
      objectName: "extension manifest",
      object: aManifest,
      property: "manifest_version",
      expectedType: "number",
      trim: true
    }

    const nameSpec = {
      objectName: "extension manifest",
      object: aManifest,
      property: "name",
      expectedType: "string",
      trim: true
    }

    const versionSpec = {
      objectName: "extension manifest",
      object: aManifest,
      property: "version",
      expectedType: "string",
      trim: true
    }

    let res =
      extractor.extractValue(manifestVersionSpec) !== undefined &&
      extractor.extractValue(nameSpec)  !== undefined &&
      extractor.extractValue(versionSpec) !== undefined;

    return res;
  },

  // Converts a chrome extension manifest into a webapp manifest.
  convertManifest: function(aManifest) {
    if (!aManifest) {
      return null;
    }

    // Set the type to privileged to ensure we only allow signed addons.
    let result = {
      "type": "privileged",
      "name": aManifest.name,
      "role": "addon"
    }

    if (aManifest.description) {
      result.description = aManifest.description;
    }

    if (aManifest.icons) {
      result.icons = aManifest.icons;
    }

    // chrome extension manifests have a single 'author' property, that we
    // map to 'developer.name'.
    // Note that it has to match the one in the mini-manifest.
    if (aManifest.author) {
      result.developer = {
        name: aManifest.author
      }
    }

    return result;
  },

  init: function() {
    this._enabled = false;
    try {
      this._enabled = Services.prefs.getBoolPref("dom.apps.customization.enabled");
    } catch(e) {}
  },
}

UserCustomizations.init();
