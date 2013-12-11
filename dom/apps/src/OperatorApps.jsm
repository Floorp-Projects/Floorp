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

// Single Variant source dir will be set in PREF_SINGLE_VARIANT_DIR
// preference.
// if PREF_SINGLE_VARIANT_DIR does not exist or has not value, it will use (as
// single variant source) the value of
// DIRECTORY_NAME + "/" + SINGLE_VARIANT_SOURCE_DIR value instead.
// SINGLE_VARIANT_CONF_FILE will be stored on Single Variant Source.
// Apps will be stored on an app per directory basis, hanging from
// SINGLE_VARIANT_SOURCE_DIR
const DIRECTORY_NAME = "webappsDir";
const SINGLE_VARIANT_SOURCE_DIR = "svoperapps";
const SINGLE_VARIANT_CONF_FILE  = "singlevariantconf.json";
const PREF_FIRST_RUN_WITH_SIM   = "dom.webapps.firstRunWithSIM";
const PREF_SINGLE_VARIANT_DIR   = "dom.mozApps.single_variant_sourcedir";
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
let File = OS.File;
let Path = OS.Path;

let iccListener = {
  notifyStkCommand: function() {},

  notifyStkSessionEnd: function() {},

  notifyCardStateChanged: function() {},

  notifyIccInfoChanged: function() {
    // TODO: Bug 927709 - OperatorApps for multi-sim
    // In Multi-sim, there is more than one client in iccProvider. Each
    // client represents a icc service. To maintain the backward compatibility
    // with single sim, we always use client 0 for now. Adding support for
    // multiple sim will be addressed in bug 927709, if needed.
    let clientId = 0;
    let iccInfo = iccProvider.getIccInfo(clientId);
    if (iccInfo && iccInfo.mcc && iccInfo.mnc) {
      debug("******* iccListener cardIccInfo MCC-MNC: " + iccInfo.mcc +
            "-" + iccInfo.mnc);
      iccProvider.unregisterIccMsg(clientId, this);
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
      Task.spawn(function() {
        try {
          yield this._initializeSourceDir();
          // TODO: Bug 927709 - OperatorApps for multi-sim
          // In Multi-sim, there is more than one client in iccProvider. Each
          // client represents a icc service. To maintain the backward
          // compatibility with single sim, we always use client 0 for now.
          // Adding support for multiple sim will be addressed in bug 927709, if
          // needed.
          let clientId = 0;
          let iccInfo = iccProvider.getIccInfo(clientId);
          let mcc = 0;
          let mnc = 0;
          if (iccInfo && iccInfo.mcc) {
            mcc = iccInfo.mcc;
          }
          if (iccInfo && iccInfo.mnc) {
            mnc = iccInfo.mnc;
          }
          if (mcc && mnc) {
            this._installOperatorApps(mcc, mnc);
          } else {
            iccProvider.registerIccMsg(clientId, iccListener);
          }
        } catch (e) {
          debug("Error Initializing OperatorApps. " + e);
        }
      }.bind(this));
    } else {
      debug("No First Run with SIM");
    }
#endif
  },

  _copyDirectory: function(aOrg, aDst) {
    debug("copying " + aOrg + " to " + aDst);
    return aDst && Task.spawn(function() {
      try {
        let orgInfo = yield File.stat(aOrg);
        if (!orgInfo.isDir) {
          return;
        }

        let dirDstExists = yield File.exists(aDst);
        if (!dirDstExists) {
          yield File.makeDir(aDst);
        }
        let iterator = new File.DirectoryIterator(aOrg);
        if (!iterator) {
          debug("No iterator over: " + aOrg);
          return;
        }
        try {
          while (true) {
            let entry;
            try {
              entry = yield iterator.next();
            } catch (ex if ex == StopIteration) {
              break;
            }

            if (!entry.isDir) {
              yield File.copy(entry.path, Path.join(aDst, entry.name));
            } else {
              yield this._copyDirectory(entry.path,
                                        Path.join(aDst, entry.name));
            }
          }
        } finally {
          iterator.close();
        }
      } catch (e) {
        debug("Error copying " + aOrg + " to " + aDst + ". " + e);
      }
    }.bind(this));
  },

  _initializeSourceDir: function() {
    return Task.spawn(function() {
      let svFinalDirName;
      try {
        svFinalDirName = Services.prefs.getCharPref(PREF_SINGLE_VARIANT_DIR);
      } catch(e) {
        debug ("Error getting pref. " + e);
        this.appsDir = FileUtils.getFile(DIRECTORY_NAME,
                                         [SINGLE_VARIANT_SOURCE_DIR]).path;
        return;
      }
      // If SINGLE_VARIANT_CONF_FILE is in PREF_SINGLE_VARIANT_DIR return
      // PREF_SINGLE_VARIANT_DIR as sourceDir, else go to
      // DIRECTORY_NAME + SINGLE_VARIANT_SOURCE_DIR and move all apps (and
      // configuration file) to PREF_SINGLE_VARIANT_DIR and return
      // PREF_SINGLE_VARIANT_DIR as sourceDir.
      let existsDir = yield File.exists(svFinalDirName);
      if (!existsDir) {
        yield File.makeDir(svFinalDirName, {ignoreExisting: true});
      }

      let existsSvIndex = yield File.exists(Path.join(svFinalDirName,
                                            SINGLE_VARIANT_CONF_FILE));
      if (!existsSvIndex) {
        let svSourceDirName = FileUtils.getFile(DIRECTORY_NAME,
                                              [SINGLE_VARIANT_SOURCE_DIR]).path;
        yield this._copyDirectory(svSourceDirName, svFinalDirName);
        debug("removing directory:" + svSourceDirName);
        File.removeDir(svSourceDirName, {
          ignoreAbsent: true,
          ignorePermissions: true
        });
      }
      this.appsDir = svFinalDirName;
    }.bind(this));
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
