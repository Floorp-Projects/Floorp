/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["OperatorAppsRegistry"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

#ifdef MOZ_B2G_RIL
XPCOMUtils.defineLazyServiceGetter(this, "iccProvider",
                                   "@mozilla.org/ril/content-helper;1",
                                   "nsIIccProvider");
#endif

function debug(aMsg) {
  //dump("-*-*- OperatorApps.jsm : " + aMsg + "\n");
}

const DIRECTORY_NAME = "webappsDir";

// The files will be stored on DIRECTORY_NAME + "/" + SINGLE_VARIANT_SOURCE_DIR
// SINGLE_VARIANT_CONF_FILE will be stored on SINGLE_VARIANT_SOURCE_DIR
// Apps will be stored on a app per directory basis, hanging from
// SINGLE_VARIANT_SOURCE_DIR
const SINGLE_VARIANT_SOURCE_DIR = "svoperapps";
const SINGLE_VARIANT_CONF_FILE  = "singlevariantconf.json";
const PREF_FIRST_RUN_WITH_SIM   = "dom.webapps.firstRunWithSIM";
const METADATA                  = "metadata.json";
const UPDATEMANIFEST            = "update.webapp";
const MANIFEST                  = "manifest.webapp";
const APPLICATION_ZIP           = "application.zip";

function isFirstRunWithSIM() {
  try {
    if (Services.prefs.prefHasUserValue(PREF_FIRST_RUN_WITH_SIM)) {
      return Services.prefs.getBoolPref(PREF_FIRST_RUN_WITH_SIM);
    }
  } catch(e) {
    debug ("Error getting pref. " + e);
  }
  return true;
}

#ifdef MOZ_B2G_RIL
let iccListener = {
  notifyStkCommand: function() {},

  notifyStkSessionEnd: function() {},

  notifyCardStateChanged: function() {},

  notifyIccInfoChanged: function() {
    let iccInfo = iccProvider.iccInfo;
    if (iccInfo && iccInfo.mcc && iccInfo.mnc) {
      debug("******* iccListener cardIccInfo MCC-MNC: " + iccInfo.mcc +
            "-" + iccInfo.mnc);
      iccProvider.unregisterIccMsg(this);
      OperatorAppsRegistry._installOperatorApps(iccInfo.mcc, iccInfo.mnc);
    }
  }
};
#endif

this.OperatorAppsRegistry = {

  _baseDirectory: null,

  init: function() {
    debug("init");
#ifdef MOZ_B2G_RIL
    if (isFirstRunWithSIM()) {
      debug("First Run with SIM");
      let mcc = 0;
      let mnc = 0;
      if (iccProvider.iccInfo && iccProvider.iccInfo.mcc) {
        mcc = iccProvider.iccInfo.mcc;
      }
      if (iccProvider.iccInfo && iccProvider.iccInfo.mnc) {
        mnc = iccProvider.iccInfo.mnc;
      }
      if (mcc && mnc) {
        this._installOperatorApps(mcc, mnc);
      } else {
        iccProvider.registerIccMsg(iccListener);
      }
    } else {
      debug("No First Run with SIM");
    }
#endif
  },

  set appsDir(aDir) {
    debug("appsDir SET: " + aDir);
    if (aDir) {
      this._baseDirectory = Cc["@mozilla.org/file/local;1"]
          .createInstance(Ci.nsILocalFile);
      this._baseDirectory.initWithPath(aDir);
    } else {
      this._baseDirectory = null;
    }
  },

  get appsDir() {
    if (!this._baseDirectory) {
      this._baseDirectory = FileUtils.getFile(DIRECTORY_NAME,
                                              [SINGLE_VARIANT_SOURCE_DIR]);
    }
    return this._baseDirectory;
  },

  eraseVariantAppsNotInList: function(aIdsApp) {
    if (!aIdsApp || !Array.isArray(aIdsApp)) {
      aIdsApp = [ ];
    }

    let svDir;
    try {
      svDir = this.appsDir.clone();
    } catch (e) {
      debug("eraseVariantAppsNotInList --> Error getting Dir "+
             svDir.path + ". " + e);
      return;
    }

    if (!svDir || !svDir.exists()) {
      return;
    }

    let entries = svDir.directoryEntries;
    while (entries.hasMoreElements()) {
      let entry = entries.getNext().QueryInterface(Ci.nsIFile);
      if (entry.isDirectory() && aIdsApp.indexOf(entry.leafName) < 0) {
        try{
          entry.remove(true);
        } catch(e) {
          debug("Error removing [" + entry.path + "]." + e);
        }
      }
    }
  },

  _launchInstall: function(isPackage, aId, aMetadata, aManifest) {
    if (!aManifest) {
      debug("Error: The application " + aId + " does not have a manifest");
      return;
    }

    let appData = {
      app: {
        installOrigin: aMetadata.installOrigin,
        origin: aMetadata.origin,
        manifestURL: aMetadata.manifestURL,
        manifestHash: AppsUtils.computeHash(JSON.stringify(aManifest))
      },
      appId: undefined,
      isBrowser: false,
      isPackage: isPackage,
      forceSuccessAck: true
    };

    if (isPackage) {
      debug("aId:" + aId + ". Installing as packaged app.");
      let installPack = OS.Path.join(this.appsDir.path, aId, APPLICATION_ZIP);
      OS.File.exists(installPack).then(
        function(aExists) {
          if (!aExists) {
            debug("SV " + installPack.path + " file do not exists for app " +
                  aId);
            return;
          }
          appData.app.localInstallPath = installPack;
          appData.app.updateManifest = aManifest;
          DOMApplicationRegistry.confirmInstall(appData);
      });
    } else {
      debug("aId:" + aId + ". Installing as hosted app.");
      appData.app.manifest = aManifest;
      DOMApplicationRegistry.confirmInstall(appData);
    }
  },

  _installOperatorApps: function(aMcc, aMnc) {
    Task.spawn(function() {
      debug("Install operator apps ---> mcc:"+ aMcc + ", mnc:" + aMnc);
      if (!isFirstRunWithSIM()) {
        debug("Operator apps already installed.");
        return;
      }

      let aIdsApp = yield this._getSingleVariantApps(aMcc, aMnc);
      debug("installOperatorApps --> aIdsApp:" + JSON.stringify(aIdsApp));
      for (let i = 0; i < aIdsApp.length; i++) {
        let aId = aIdsApp[i];
        let aMetadata = yield AppsUtils.loadJSONAsync(
                           OS.Path.join(this.appsDir.path, aId, METADATA));
        debug("metadata:" + JSON.stringify(aMetadata));
        let isPackage = true;
        let manifest;
        let manifests = [UPDATEMANIFEST, MANIFEST];
        for (let j = 0; j < manifests.length; j++) {
          try {
            manifest = yield AppsUtils.loadJSONAsync(
                          OS.Path.join(this.appsDir.path, aId, manifests[j]));
            break;
          } catch (e) {
            isPackage = false;
          }
        }
        if (manifest) {
          this._launchInstall(isPackage, aId, aMetadata, manifest);
        } else {
          debug ("Error. Neither " + UPDATEMANIFEST + " file nor " + MANIFEST +
                 " file for " + aId + " app.");
        }
      }
      this.eraseVariantAppsNotInList(aIdsApp);
      Services.prefs.setBoolPref(PREF_FIRST_RUN_WITH_SIM, false);
      Services.prefs.savePrefFile(null);
    }.bind(this)).then(null, function(aError) {
        debug("Error: " + aError);
    });
  },

  _getSingleVariantApps: function(aMcc, aMnc) {

    function normalizeCode(aCode) {
      let ncode = "" + aCode;
      while (ncode.length < 3) {
        ncode = "0" + ncode;
      }
      return ncode;
    }

    return Task.spawn(function () {
      let key = normalizeCode(aMcc) + "-" + normalizeCode(aMnc);
      let file = OS.Path.join(this.appsDir.path, SINGLE_VARIANT_CONF_FILE);
      let aData = yield AppsUtils.loadJSONAsync(file);
      if (!aData || !(key in aData)) {
        return;
      }
      throw new Task.Result(aData[key]);
    }.bind(this));
  }
};

OperatorAppsRegistry.init();
