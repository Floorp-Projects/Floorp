/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function debug(aMsg) {
  // dump("-- InterAppCommService: " + Date.now() + ": " + aMsg + "\n");
}

function InterAppCommService() {
  Services.obs.addObserver(this, "xpcom-shutdown", false);

  // This matrix is used for saving the inter-app connection info registered in
  // the app manifest. The object literal is defined as below:
  //
  // {
  //   "keyword1": {
  //     "subAppManifestURL1": {
  //       /* subscribed info */
  //     },
  //     "subAppManifestURL2": {
  //       /* subscribed info */
  //     },
  //     ...
  //   },
  //   "keyword2": {
  //     "subAppManifestURL3": {
  //       /* subscribed info */
  //     },
  //     ...
  //   },
  //   ...
  // }
  //
  // For example:
  //
  // {
  //   "foo": {
  //     "app://subApp1.gaiamobile.org/manifest.webapp": {
  //       pageURL: "app://subApp1.gaiamobile.org/handler.html",
  //       description: "blah blah",
  //       appStatus: Ci.nsIPrincipal.APP_STATUS_CERTIFIED,
  //       rules: { ... }
  //     },
  //     "app://subApp2.gaiamobile.org/manifest.webapp": {
  //       pageURL: "app://subApp2.gaiamobile.org/handler.html",
  //       description: "blah blah",
  //       appStatus: Ci.nsIPrincipal.APP_STATUS_PRIVILEGED,
  //       rules: { ... }
  //     }
  //   },
  //   "bar": {
  //     "app://subApp3.gaiamobile.org/manifest.webapp": {
  //       pageURL: "app://subApp3.gaiamobile.org/handler.html",
  //       description: "blah blah",
  //       appStatus: Ci.nsIPrincipal.APP_STATUS_INSTALLED,
  //       rules: { ... }
  //     }
  //   }
  // }
  //
  // TODO Bug 908999 - Update registered connections when app gets uninstalled.
  this._registeredConnections = {};
}

InterAppCommService.prototype = {
  registerConnection: function(aKeyword, aHandlerPageURI, aManifestURI,
                               aDescription, aAppStatus, aRules) {
    let manifestURL = aManifestURI.spec;
    let pageURL = aHandlerPageURI.spec;

    debug("registerConnection: aKeyword: " + aKeyword +
          " manifestURL: " + manifestURL + " pageURL: " + pageURL +
          " aDescription: " + aDescription + " aAppStatus: " + aAppStatus +
          " aRules.minimumAccessLevel: " + aRules.minimumAccessLevel +
          " aRules.manifestURLs: " + aRules.manifestURLs +
          " aRules.installOrigins: " + aRules.installOrigins);

    let subAppManifestURLs = this._registeredConnections[aKeyword];
    if (!subAppManifestURLs) {
      subAppManifestURLs = this._registeredConnections[aKeyword] = {};
    }

    subAppManifestURLs[manifestURL] = {
      pageURL: pageURL,
      description: aDescription,
      appStatus: aAppStatus,
      rules: aRules,
      manifestURL: manifestURL
    };
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "xpcom-shutdown");
        break;
    }
  },

  classID: Components.ID("{3dd15ce6-e7be-11e2-82bc-77967e7a63e6}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIInterAppCommService,
                                         Ci.nsIObserver])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([InterAppCommService]);
