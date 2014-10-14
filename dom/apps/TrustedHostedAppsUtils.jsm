/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global Components, Services, dump, AppsUtils, NetUtil, XPCOMUtils */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const signatureFileExtension = ".sig";

this.EXPORTED_SYMBOLS = ["TrustedHostedAppsUtils"];

Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

#ifdef MOZ_WIDGET_ANDROID
// On Android, define the "debug" function as a binding of the "d" function
// from the AndroidLog module so it gets the "debug" priority and a log tag.
// We always report debug messages on Android because it's unnecessary
// to restrict reporting, per bug 1003469.
let debug = Cu
  .import("resource://gre/modules/AndroidLog.jsm", {})
  .AndroidLog.d.bind(null, "TrustedHostedAppsUtils");
#else
// Elsewhere, report debug messages only if dom.mozApps.debug is set to true.
// The pref is only checked once, on startup, so restart after changing it.
let debug = Services.prefs.getBoolPref("dom.mozApps.debug") ?
  aMsg => dump("-*- TrustedHostedAppsUtils.jsm : " + aMsg + "\n") :
  () => {};
#endif

/**
 * Verification functions for Trusted Hosted Apps.
 */
this.TrustedHostedAppsUtils = {

  /**
   * Check if the given host is pinned in the CA pinning database.
   */
  isHostPinned: function (aUrl) {
    let uri;
    try {
      uri = Services.io.newURI(aUrl, null, null);
    } catch(e) {
      debug("Host parsing failed: " + e);
      return false;
    }

    // TODO: use nsSiteSecurityService.isSecureURI()
    if (!uri.host || "https" != uri.scheme) {
      return false;
    }

    // Check certificate pinning
    let siteSecurityService;
    try {
      siteSecurityService = Cc["@mozilla.org/ssservice;1"]
        .getService(Ci.nsISiteSecurityService);
    } catch (e) {
      debug("nsISiteSecurityService error: " + e);
      // unrecoverable error, don't bug the user
      throw "CERTDB_ERROR";
    }

    if (siteSecurityService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                         uri.host, 0)) {
      debug("\tvalid certificate pinning for host: " + uri.host + "\n");
      return true;
    }

    debug("\tHost NOT pinned: " + uri.host + "\n");
    return false;
  },

  /**
   * Take a CSP policy string as input and ensure that it contains at
   * least the directives that are required ('script-src' and
   * 'style-src').  If the CSP policy string is 'undefined' or does
   * not contain some of the required csp directives the function will
   * return empty list with status set to false.  Otherwise a parsed
   * list of the unique sources listed from the required csp
   * directives is returned.
   */
  getCSPWhiteList: function(aCsp) {
    let isValid = false;
    let whiteList = [];
    let requiredDirectives = [ "script-src", "style-src" ];

    if (aCsp) {
      let validDirectives = [];
      let directives = aCsp.split(";");
      // TODO: Use nsIContentSecurityPolicy
      directives
        .map(aDirective => aDirective.trim().split(" "))
        .filter(aList => aList.length > 1)
        // we only restrict on requiredDirectives
        .filter(aList => (requiredDirectives.indexOf(aList[0]) != -1))
        .forEach(aList => {
          // aList[0] contains the directive name.
          // aList[1..n] contains sources.
          let directiveName = aList.shift();
          let sources = aList;

          if ((-1 == validDirectives.indexOf(directiveName))) {
            validDirectives.push(directiveName);
          }
          whiteList.push(...sources.filter(
             // 'self' is checked separately during manifest check
            aSource => (aSource !="'self'" && whiteList.indexOf(aSource) == -1)
          ));
        });

      // Check if all required directives are present.
      isValid = requiredDirectives.length === validDirectives.length;

      if (!isValid) {
        debug("White list doesn't contain all required directives!");
        whiteList = [];
      }
    }

    debug("White list contains " + whiteList.length + " hosts");
    return { list: whiteList, valid: isValid };
  },

  /**
   * Verify that the given csp is valid:
   *  1. contains required directives "script-src" and "style-src"
   *  2. required directives contain only "https" URLs
   *  3. domains of the restricted sources exist in the CA pinning database
   */
  verifyCSPWhiteList: function(aCsp) {
    let domainWhitelist = this.getCSPWhiteList(aCsp);
    if (!domainWhitelist.valid) {
      debug("TRUSTED_APPLICATION_WHITELIST_PARSING_FAILED");
      return false;
    }

    if (!domainWhitelist.list.every(aUrl => this.isHostPinned(aUrl))) {
      debug("TRUSTED_APPLICATION_WHITELIST_VALIDATION_FAILED");
      return false;
    }

    return true;
  },

  _verifySignedFile: function(aManifestStream, aSignatureStream, aCertDb) {
    let deferred = Promise.defer();

    let root = Ci.nsIX509CertDB.TrustedHostedAppPublicRoot;
    try {
      // Check if we should use the test certificates.
      // Please note that this should be changed if we ever allow chages to the
      // prefs since that would create a way for an attacker to use the test
      // root for real apps.
      let useTrustedAppTestCerts = Services.prefs
        .getBoolPref("dom.mozApps.use_trustedapp_test_certs");
      if (useTrustedAppTestCerts) {
        root = Ci.nsIX509CertDB.TrustedHostedAppTestRoot;
      }
    } catch (ex) { }

    aCertDb.verifySignedManifestAsync(
      root, aManifestStream, aSignatureStream,
      function(aRv, aCert) {
        debug("Signature verification returned code, cert & root: " + aRv + " " + aCert + " " + root);
        if (Components.isSuccessCode(aRv)) {
          deferred.resolve(aCert);
        } else if (aRv == Cr.NS_ERROR_FILE_CORRUPTED ||
                   aRv == Cr.NS_ERROR_SIGNED_MANIFEST_FILE_INVALID) {
          deferred.reject("MANIFEST_SIGNATURE_FILE_INVALID");
        } else {
          deferred.reject("MANIFEST_SIGNATURE_VERIFICATION_ERROR");
        }
      }
    );

    return deferred.promise;
  },

  verifySignedManifest: function(aApp, aAppId) {
    let deferred = Promise.defer();

    let certDb;
    try {
      certDb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
    } catch (e) {
      debug("nsIX509CertDB error: " + e);
      // unrecoverable error, don't bug the user
      throw "CERTDB_ERROR";
    }

    let mRequestChannel = NetUtil.newChannel(aApp.manifestURL)
                                 .QueryInterface(Ci.nsIHttpChannel);
    mRequestChannel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    mRequestChannel.notificationCallbacks =
      AppsUtils.createLoadContext(aAppId, false);

    // The manifest signature must be located at the same path as the
    // manifest and have the same file name, only the file extension
    // should differ. Any fragment or query parameter will be ignored.
    let signatureURL;
    try {
      let mURL = Cc["@mozilla.org/network/io-service;1"]
        .getService(Ci.nsIIOService)
        .newURI(aApp.manifestURL, null, null)
        .QueryInterface(Ci.nsIURL);
      signatureURL = mURL.prePath +
        mURL.directory + mURL.fileBaseName + signatureFileExtension;
    } catch(e) {
      deferred.reject("SIGNATURE_PATH_INVALID");
      return;
    }

    let sRequestChannel = NetUtil.newChannel(signatureURL)
      .QueryInterface(Ci.nsIHttpChannel);
    sRequestChannel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    sRequestChannel.notificationCallbacks =
      AppsUtils.createLoadContext(aAppId, false);
    let getAsyncFetchCallback = (resolve, reject) =>
        (aInputStream, aResult) => {
          if (!Components.isSuccessCode(aResult)) {
            debug("Failed to download file");
            reject("MANIFEST_FILE_UNAVAILABLE");
            return;
          }
          resolve(aInputStream);
        };

    Promise.all([
      new Promise((resolve, reject) => {
        NetUtil.asyncFetch(mRequestChannel,
                           getAsyncFetchCallback(resolve, reject));
      }),
      new Promise((resolve, reject) => {
        NetUtil.asyncFetch(sRequestChannel,
                           getAsyncFetchCallback(resolve, reject));
      })
    ]).then(([aManifestStream, aSignatureStream]) => {
      this._verifySignedFile(aManifestStream, aSignatureStream, certDb)
        .then(deferred.resolve, deferred.reject);
    }, deferred.reject);

    return deferred.promise;
  },

  verifyManifest: function(aApp, aAppId, aManifest) {
    return new Promise((resolve, reject) => {
      // sanity check on manifest host's CA (proper CA check with
      // pinning is done by regular networking code)
      if (!this.isHostPinned(aApp.manifestURL)) {
        reject("TRUSTED_APPLICATION_HOST_CERTIFICATE_INVALID");
        return;
      }
      if (!this.verifyCSPWhiteList(aManifest.csp)) {
        reject("TRUSTED_APPLICATION_WHITELIST_VALIDATION_FAILED");
        return;
      }
      this.verifySignedManifest(aApp, aAppId).then(resolve, reject);
    });
  }
};
