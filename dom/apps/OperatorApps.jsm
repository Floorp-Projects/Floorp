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

let Path = OS.Path;

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
      let mcc = iccInfo.mcc;
      let mnc = iccInfo.mnc;
      debug("******* iccListener cardIccInfo MCC-MNC: " + mcc + "-" + mnc);
      iccProvider.unregisterIccMsg(clientId, this);
      OperatorAppsRegistry._installOperatorApps(mcc, mnc);

      debug("Broadcast message first-run-with-sim");
      let messenger = Cc["@mozilla.org/system-message-internal;1"]
                        .getService(Ci.nsISystemMessagesInternal);
      messenger.broadcastMessage("first-run-with-sim", { mcc: mcc,
                                                         mnc: mnc });
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
            let messenger = Cc["@mozilla.org/system-message-internal;1"]
                              .getService(Ci.nsISystemMessagesInternal);
            messenger.broadcastMessage("first-run-with-sim", { mcc: mcc,
                                                               mnc: mnc });

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
        let orgDir = Cc["@mozilla.org/file/local;1"]
                       .createInstance(Ci.nsIFile);
        orgDir.initWithPath(aOrg);
        if (!orgDir.exists() || !orgDir.isDirectory()) {
          debug(aOrg + " does not exist or is not a directory");
          return;
        }

        let dstDir = Cc["@mozilla.org/file/local;1"]
                       .createInstance(Ci.nsIFile);
        dstDir.initWithPath(aDst);
        if (!dstDir.exists()) {
          dstDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
        }

        let entries = orgDir.directoryEntries;
        while (entries.hasMoreElements()) {
          let entry = entries.getNext().QueryInterface(Ci.nsIFile);

          if (!entry.isDirectory()) {
            // Remove the file, because copyTo doesn't overwrite files.
            let dstFile = dstDir.clone();
            dstFile.append(entry.leafName);
            if(dstFile.exists()) {
              dstFile.remove(false);
            }

            entry.copyTo(dstDir, entry.leafName);
          } else {
            yield this._copyDirectory(entry.path,
                                      Path.join(aDst, entry.leafName));
          }
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
      let svFinalDir = Cc["@mozilla.org/file/local;1"]
                         .createInstance(Ci.nsIFile);
      svFinalDir.initWithPath(svFinalDirName);
      if (!svFinalDir.exists()) {
        svFinalDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
      }

      let svIndex = svFinalDir.clone();
      svIndex.append(SINGLE_VARIANT_CONF_FILE);
      if (!svIndex.exists()) {
        let svSourceDir = FileUtils.getFile(DIRECTORY_NAME,
                                            [SINGLE_VARIANT_SOURCE_DIR]);

        yield this._copyDirectory(svSourceDir.path, svFinalDirName);

        debug("removing directory:" + svSourceDir.path);
        try {
          svSourceDir.remove(true);
        } catch(ex) { }
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
    if (!aIdsApp) {
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
      let installPack = this.appsDir.clone();
      installPack.append(aId);
      installPack.append(APPLICATION_ZIP);

      if (!installPack.exists()) {
        debug("SV " + installPack.path + " file do not exists for app " + aId);
        return;
      }

      appData.app.localInstallPath = installPack.path;
      appData.app.updateManifest = aManifest;
      DOMApplicationRegistry.confirmInstall(appData);
    } else {
      debug("aId:" + aId + ". Installing as hosted app.");
      appData.app.manifest = aManifest;
      DOMApplicationRegistry.confirmInstall(appData);
    }
  },

  _installOperatorApps: function(aMcc, aMnc) {
    function normalizeCode(aCode) {
      let ncode = "" + aCode;
      while (ncode.length < 3) {
        ncode = "0" + ncode;
      }
      return ncode;
    }

    Task.spawn(function() {
      debug("Install operator apps ---> mcc:"+ aMcc + ", mnc:" + aMnc);
      if (!isFirstRunWithSIM()) {
        debug("Operator apps already installed.");
        return;
      }

      let key = normalizeCode(aMcc) + "-" + normalizeCode(aMnc);
      let aIdsApp = yield this._getSingleVariantDatas();

      // aIdsApp will be undefined if the singleVariant config file not exist
      // or will have the following format:
      // {"mmc1-mnc1": [ap11,...,ap1N],..., "mmcM-mncM": [apM1,...,apMN]}
      // Behavior:
      // * If the configuration file does not exist, it's equivalent to
      //   passing []
      // * If the configuration file has data and the phone boots with a SIM
      //   that isn't on the configuration file then we must have the same
      //   behavior as if the phone had booted without a SIM inserted
      //   (that is, don't do anything)
      // * If the phone boots with a configured SIM (mcc-mnc exists on
      //   configuration file) then recover the app list to install
      if (!aIdsApp) {
        debug("No " + SINGLE_VARIANT_CONF_FILE + " in " + this.appsDir.path);
        aIdsApp = [];
      } else if (aIdsApp[key] === undefined) {
        debug("First Run with SIM not configured");
        return;
      } else {
        debug("First Run with configured SIM ");
        aIdsApp = aIdsApp[key];
        if (!Array.isArray(aIdsApp)) {
          aIdsApp = [aIdsApp];
        }
      }

      debug("installOperatorApps --> aIdsApp:" + JSON.stringify(aIdsApp));
      for (let i = 0; i < aIdsApp.length; i++) {
        let aId = aIdsApp[i];
        let aMetadata = yield AppsUtils.loadJSONAsync(
                           Path.join(this.appsDir.path, aId, METADATA));
        if (!aMetadata) {
          debug("Error reading metadata file");
          return;
        }

        debug("metadata:" + JSON.stringify(aMetadata));
        let isPackage = true;
        let manifest;
        let manifests = [UPDATEMANIFEST, MANIFEST];
        for (let j = 0; j < manifests.length; j++) {
          manifest = yield AppsUtils.loadJSONAsync(
                        Path.join(this.appsDir.path, aId, manifests[j]));

          if (!manifest) {
            isPackage = false;
          } else {
            break;
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

  _getSingleVariantDatas: function() {
    return Task.spawn(function*() {
      let file = Path.join(this.appsDir.path, SINGLE_VARIANT_CONF_FILE);
      let aData = yield AppsUtils.loadJSONAsync(file);
      return aData;
    }.bind(this));
  }
};

OperatorAppsRegistry.init();
