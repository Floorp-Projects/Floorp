/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

// Possible errors thrown by the signature verifier.
const SEC_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;
const SEC_ERROR_EXPIRED_CERTIFICATE = (SEC_ERROR_BASE + 11);

// We need this to decide if we should accept or not files signed with expired
// certificates.
function buildIDToTime() {
  let platformBuildID =
    Cc["@mozilla.org/xre/app-info;1"]
      .getService(Ci.nsIXULAppInfo).platformBuildID;
  let platformBuildIDDate = new Date();
  platformBuildIDDate.setUTCFullYear(platformBuildID.substr(0,4),
                                      platformBuildID.substr(4,2) - 1,
                                      platformBuildID.substr(6,2));
  platformBuildIDDate.setUTCHours(platformBuildID.substr(8,2),
                                  platformBuildID.substr(10,2),
                                  platformBuildID.substr(12,2));
  return platformBuildIDDate.getTime();
}

const PLATFORM_BUILD_ID_TIME = buildIDToTime();

this.EXPORTED_SYMBOLS = ["DOMApplicationRegistry"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import('resource://gre/modules/ActivitiesService.jsm');
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/AppDownloadManager.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyGetter(this, "UserCustomizations", function() {
  let enabled = false;
  try {
    enabled = Services.prefs.getBoolPref("dom.apps.customization.enabled");
  } catch(e) {}

  if (enabled) {
    return Cu.import("resource://gre/modules/UserCustomizations.jsm", {})
             .UserCustomizations;
  } else {
    return {
      register: function() {},
      unregister: function() {}
    };
  }
});

XPCOMUtils.defineLazyModuleGetter(this, "TrustedRootCertificate",
  "resource://gre/modules/StoreTrustAnchor.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PermissionsInstaller",
  "resource://gre/modules/PermissionsInstaller.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OfflineCacheInstaller",
  "resource://gre/modules/OfflineCacheInstaller.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SystemMessagePermissionsChecker",
  "resource://gre/modules/SystemMessagePermissionsChecker.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebappOSUtils",
  "resource://gre/modules/WebappOSUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ScriptPreloader",
                                  "resource://gre/modules/ScriptPreloader.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Langpacks",
                                  "resource://gre/modules/Langpacks.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "TrustedHostedAppsUtils",
                                  "resource://gre/modules/TrustedHostedAppsUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ImportExport",
                                  "resource://gre/modules/ImportExport.jsm");

#ifdef MOZ_WIDGET_GONK
XPCOMUtils.defineLazyGetter(this, "libcutils", function() {
  Cu.import("resource://gre/modules/systemlibs.js");
  return libcutils;
});
#endif

#ifdef MOZ_WIDGET_ANDROID
// On Android, define the "debug" function as a binding of the "d" function
// from the AndroidLog module so it gets the "debug" priority and a log tag.
// We always report debug messages on Android because it's unnecessary
// to restrict reporting, per bug 1003469.
let debug = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
              .AndroidLog.d.bind(null, "Webapps");
#else
// Elsewhere, report debug messages only if dom.mozApps.debug is set to true.
// The pref is only checked once, on startup, so restart after changing it.
let debug = Services.prefs.getBoolPref("dom.mozApps.debug")
              ? (aMsg) => dump("-*- Webapps.jsm : " + aMsg + "\n")
              : (aMsg) => {};
#endif

function getNSPRErrorCode(err) {
  return -1 * ((err) & 0xffff);
}

function supportUseCurrentProfile() {
  return Services.prefs.getBoolPref("dom.webapps.useCurrentProfile");
}

function supportSystemMessages() {
  return Services.prefs.getBoolPref("dom.sysmsg.enabled");
}

// Minimum delay between two progress events while downloading, in ms.
const MIN_PROGRESS_EVENT_DELAY = 1500;

const WEBAPP_RUNTIME = Services.appinfo.ID == "webapprt@mozilla.org";

const chromeWindowType = WEBAPP_RUNTIME ? "webapprt:webapp" : "navigator:browser";

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

XPCOMUtils.defineLazyGetter(this, "interAppCommService", function() {
  return Cc["@mozilla.org/inter-app-communication-service;1"]
         .getService(Ci.nsIInterAppCommService);
});

XPCOMUtils.defineLazyServiceGetter(this, "dataStoreService",
                                   "@mozilla.org/datastore-service;1",
                                   "nsIDataStoreService");

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

XPCOMUtils.defineLazyGetter(this, "msgmgr", function() {
  return Cc["@mozilla.org/system-message-internal;1"]
         .getService(Ci.nsISystemMessagesInternal);
});

XPCOMUtils.defineLazyGetter(this, "updateSvc", function() {
  return Cc["@mozilla.org/offlinecacheupdate-service;1"]
           .getService(Ci.nsIOfflineCacheUpdateService);
});

XPCOMUtils.defineLazyGetter(this, "permMgr", function() {
  return Cc["@mozilla.org/permissionmanager;1"]
           .getService(Ci.nsIPermissionManager);
});

#ifdef MOZ_WIDGET_GONK
  const DIRECTORY_NAME = "webappsDir";
#elifdef ANDROID
  const DIRECTORY_NAME = "webappsDir";
#else
  // If we're executing in the context of the webapp runtime, the data files
  // are in a different directory (currently the Firefox profile that installed
  // the webapp); otherwise, they're in the current profile.
  const DIRECTORY_NAME = WEBAPP_RUNTIME ? "WebappRegD" : "ProfD";
#endif

// We'll use this to identify privileged apps that have been preinstalled
// For those apps we'll set
// STORE_ID_PENDING_PREFIX + installOrigin
// as the storeID. This ensures it's unique and can't be set from a legit
// store even by error.
const STORE_ID_PENDING_PREFIX = "#unknownID#";

this.DOMApplicationRegistry = {
  // pseudo-constants for the different application kinds.
  get kPackaged()       "packaged",
  get kHosted()         "hosted",
  get kHostedAppcache() "hosted-appcache",
  get kTrustedHosted()  "hosted-trusted",

  // Path to the webapps.json file where we store the registry data.
  appsFile: null,
  webapps: { },
  children: [ ],
  allAppsLaunchable: false,
  _updateHandlers: [ ],
  _pendingUninstalls: {},
  _contentActions: new Map(),
  dirKey: DIRECTORY_NAME,

  init: function() {
    this.messages = ["Webapps:Install",
                     "Webapps:Uninstall",
                     "Webapps:GetSelf",
                     "Webapps:CheckInstalled",
                     "Webapps:GetInstalled",
                     "Webapps:GetNotInstalled",
                     "Webapps:Launch",
                     "Webapps:LocationChange",
                     "Webapps:InstallPackage",
                     "Webapps:GetList",
                     "Webapps:RegisterForMessages",
                     "Webapps:UnregisterForMessages",
                     "Webapps:CancelDownload",
                     "Webapps:CheckForUpdate",
                     "Webapps:Download",
                     "Webapps:ApplyDownload",
                     "Webapps:Install:Return:Ack",
                     "Webapps:AddReceipt",
                     "Webapps:RemoveReceipt",
                     "Webapps:ReplaceReceipt",
                     "Webapps:RegisterBEP",
                     "Webapps:Export",
                     "Webapps:Import",
                     "Webapps:GetIcon",
                     "Webapps:ExtractManifest",
                     "Webapps:SetEnabled",
                     "child-process-shutdown"];

    this.frameMessages = ["Webapps:ClearBrowserData"];

    this.messages.forEach((function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }).bind(this));

    cpmm.addMessageListener("Activities:Register:OK", this);
    cpmm.addMessageListener("Activities:Register:KO", this);

    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, "memory-pressure", false);

    AppDownloadManager.registerCancelFunction(this.cancelDownload.bind(this));

    this.appsFile = FileUtils.getFile(DIRECTORY_NAME,
                                      ["webapps", "webapps.json"], true).path;

    this.loadAndUpdateApps();

    Langpacks.registerRegistryFunctions(this.broadcastMessage.bind(this),
                                        this._appIdForManifestURL.bind(this),
                                        this.getFullAppByManifestURL.bind(this));
  },

  // loads the current registry, that could be empty on first run.
  loadCurrentRegistry: function() {
    return AppsUtils.loadJSONAsync(this.appsFile).then((aData) => {
      if (!aData) {
        return;
      }

      this.webapps = aData;
      let appDir = OS.Path.dirname(this.appsFile);
      for (let id in this.webapps) {
        let app = this.webapps[id];
        if (!app) {
          delete this.webapps[id];
          continue;
        }

        app.id = id;

        // Make sure we have a localId
        if (app.localId === undefined) {
          app.localId = this._nextLocalId();
        }

        if (app.basePath === undefined) {
          app.basePath = appDir;
        }

        // Default to removable apps.
        if (app.removable === undefined) {
          app.removable = true;
        }

        // Default to a non privileged status.
        if (app.appStatus === undefined) {
          app.appStatus = Ci.nsIPrincipal.APP_STATUS_INSTALLED;
        }

        // Default to NO_APP_ID and not in browser.
        if (app.installerAppId === undefined) {
          app.installerAppId = Ci.nsIScriptSecurityManager.NO_APP_ID;
        }
        if (app.installerIsBrowser === undefined) {
          app.installerIsBrowser = false;
        }

        // Default installState to "installed", and reset if we shutdown
        // during an update.
        if (app.installState === undefined ||
            app.installState === "updating") {
          app.installState = "installed";
        }

        // Default storeId to "" and storeVersion to 0
        if (app.storeId === undefined) {
          app.storeId = "";
        }
        if (app.storeVersion === undefined) {
          app.storeVersion = 0;
        }

        // Default role to "".
        if (app.role === undefined) {
          app.role = "";
        }

        if (app.widgetPages === undefined) {
          app.widgetPages = [];
        }

        if (!AppsUtils.checkAppRole(app.role, app.appStatus)) {
          delete this.webapps[id];
          continue;
        }

        if (app.enabled === undefined) {
          app.enabled = true;
        }

        // At startup we can't be downloading, and the $TMP directory
        // will be empty so we can't just apply a staged update.
        app.downloading = false;
        app.readyToApplyDownload = false;
      }
    });
  },

  // Notify we are starting with registering apps.
  _registryStarted: Promise.defer(),
  notifyAppsRegistryStart: function notifyAppsRegistryStart() {
    Services.obs.notifyObservers(this, "webapps-registry-start", null);
    this._registryStarted.resolve();
  },

  get registryStarted() {
    return this._registryStarted.promise;
  },

  // The registry will be safe to clone when this promise is resolved.
  _safeToClone: Promise.defer(),

  // Notify we are done with registering apps and save a copy of the registry.
  _registryReady: Promise.defer(),
  notifyAppsRegistryReady: function notifyAppsRegistryReady() {
    // Usually this promise will be resolved earlier, but just in case,
    // resolve it here also.
    this._safeToClone.resolve();
    this._registryReady.resolve();
    Services.obs.notifyObservers(this, "webapps-registry-ready", null);
    this._saveApps();
  },

  get registryReady() {
    return this._registryReady.promise;
  },

  get safeToClone() {
    return this._safeToClone.promise;
  },

  // Ensure that the .to property in redirects is a relative URL.
  sanitizeRedirects: function sanitizeRedirects(aSource) {
    if (!aSource) {
      return null;
    }

    let res = [];
    for (let i = 0; i < aSource.length; i++) {
      let redirect = aSource[i];
      if (redirect.from && redirect.to &&
          isAbsoluteURI(redirect.from) &&
          !isAbsoluteURI(redirect.to)) {
        res.push(redirect);
      }
    }
    return res.length > 0 ? res : null;
  },

  _saveWidgetsFullPath: function(aManifest, aDestApp) {
    if (aManifest.widgetPages) {
      let resolve = (aPage)=>{
        let filepath = AppsUtils.getFilePath(aPage);
        return aManifest.resolveURL(filepath);
      };
      aDestApp.widgetPages = aManifest.widgetPages.map(resolve);
    } else {
      aDestApp.widgetPages = [];
    }
  },

  // Registers all the activities and system messages.
  registerAppsHandlers: Task.async(function*(aRunUpdate) {
    this.notifyAppsRegistryStart();
    let ids = [];
    for (let id in this.webapps) {
      ids.push({ id: id });
    }
    if (supportSystemMessages()) {
      this._processManifestForIds(ids, aRunUpdate);
    } else {
      // Read the CSPs and roles. If MOZ_SYS_MSG is defined this is done on
      // _processManifestForIds so as to not reading the manifests
      // twice
      let results = yield this._readManifests(ids);
      results.forEach((aResult) => {
        if (!aResult.manifest) {
          // If we can't load the manifest, we probably have a corrupted
          // registry. We delete the app since we can't do anything with it.
          delete this.webapps[aResult.id];
          return;
        }
        let app = this.webapps[aResult.id];
        app.csp = aResult.manifest.csp || "";
        app.role = aResult.manifest.role || "";

        let localeManifest = new ManifestHelper(aResult.manifest, app.origin, app.manifestURL);
        this._saveWidgetsFullPath(localeManifest, app);

        if (app.appStatus >= Ci.nsIPrincipal.APP_STATUS_PRIVILEGED) {
          app.redirects = this.sanitizeRedirects(aResult.redirects);
        }
        app.kind = this.appKind(app, aResult.manifest);
        UserCustomizations.register(aResult.manifest, app);
        Langpacks.register(app, aResult.manifest);
      });

      // Nothing else to do but notifying we're ready.
      this.notifyAppsRegistryReady();
    }
  }),

  updateDataStoreForApp: Task.async(function*(aId) {
    if (!this.webapps[aId]) {
      return;
    }

    // Create or Update the DataStore for this app
    let results = yield this._readManifests([{ id: aId }]);
    let app = this.webapps[aId];
    this.updateDataStore(app.localId, app.origin, app.manifestURL,
                         results[0].manifest, app.appStatus);
  }),

  appKind: function(aApp, aManifest) {
    if (aApp.origin.startsWith("app://")) {
      return this.kPackaged;
    } else {
      // Hosted apps, can be appcached or not.
      let kind = this.kHosted;
      if (aManifest.type == "trusted") {
        kind = this.kTrustedHosted;
      } else if (aManifest.appcache_path) {
        kind = this.kHostedAppcache;
      }
      return kind;
    }
  },

  updatePermissionsForApp: function(aId, aIsPreinstalled) {
    if (!this.webapps[aId]) {
      return;
    }

    // Install the permissions for this app, as if we were updating
    // to cleanup the old ones if needed.
    // TODO It's not clear what this should do when there are multiple profiles.
    if (supportUseCurrentProfile()) {
      this._readManifests([{ id: aId }]).then((aResult) => {
        let data = aResult[0];
        this.webapps[aId].kind = this.webapps[aId].kind ||
          this.appKind(this.webapps[aId], aResult[0].manifest);
        PermissionsInstaller.installPermissions({
          manifest: data.manifest,
          manifestURL: this.webapps[aId].manifestURL,
          origin: this.webapps[aId].origin,
          isPreinstalled: aIsPreinstalled,
          kind: this.webapps[aId].kind
        }, true, function() {
          debug("Error installing permissions for " + aId);
        });
      });
    }
  },

  updateOfflineCacheForApp: function(aId) {
    let app = this.webapps[aId];
    this._readManifests([{ id: aId }]).then((aResult) => {
      let manifest =
        new ManifestHelper(aResult[0].manifest, app.origin, app.manifestURL);
      let fullAppcachePath = manifest.fullAppcachePath();
      if (fullAppcachePath) {
        OfflineCacheInstaller.installCache({
          cachePath: app.cachePath || app.basePath,
          appId: aId,
          origin: Services.io.newURI(app.origin, null, null),
          localId: app.localId,
          appcache_path: fullAppcachePath
        });
      }
    });
  },

  // Installs a 3rd party app.
  installPreinstalledApp: function installPreinstalledApp(aId) {
#ifdef MOZ_WIDGET_GONK
    // In some cases, the app might be already installed under a different ID but
    // with the same manifestURL. In that case, the only content of the webapp will
    // be the id of the old version, which is the one we'll keep.
    let destId  = this.webapps[aId].oldId || aId;
    // We don't need the oldId anymore
    if (destId !== aId) {
      delete this.webapps[aId];
    }

    let app = this.webapps[destId];
    let baseDir, isPreinstalled = false;
    try {
      baseDir = FileUtils.getDir("coreAppsDir", ["webapps", aId], false);
      if (!baseDir.exists()) {
        return isPreinstalled;
      } else if (!baseDir.directoryEntries.hasMoreElements()) {
        debug("Error: Core app in " + baseDir.path + " is empty");
        return isPreinstalled;
      }
    } catch(e) {
      // In ENG builds, we don't have apps in coreAppsDir.
      return isPreinstalled;
    }

    // Beyond this point we know it's really a preinstalled app.
    isPreinstalled = true;

    let filesToMove;
    let isPackage;

    let updateFile = baseDir.clone();
    updateFile.append("update.webapp");
    if (!updateFile.exists()) {
      // The update manifest is missing, this is a hosted app only if there is
      // no application.zip
      let appFile = baseDir.clone();
      appFile.append("application.zip");
      if (appFile.exists()) {
        return isPreinstalled;
      }

      isPackage = false;
      filesToMove = ["manifest.webapp"];
    } else {
      isPackage = true;
      filesToMove = ["application.zip", "update.webapp"];
    }

    debug("Installing 3rd party app : " + aId +
          " from " + baseDir.path + " to " + destId);

    // We copy this app to DIRECTORY_NAME/$destId, and set the base path as needed.
    let destDir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", destId], true, true);

    filesToMove.forEach(function(aFile) {
        let file = baseDir.clone();
        file.append(aFile);
        try {
          file.copyTo(destDir, aFile);
        } catch(e) {
          debug("Error: Failed to copy " + file.path + " to " + destDir.path);
        }
      });

    app.installState = "installed";
    app.cachePath = app.basePath;
    app.basePath = OS.Path.dirname(this.appsFile);

    if (!isPackage) {
      return isPreinstalled;
    }

    app.origin = "app://" + destId;

    // Do this for all preinstalled apps... we can't know at this
    // point if the updates will be signed or not and it doesn't
    // hurt to have it always.
    app.storeId = STORE_ID_PENDING_PREFIX + app.installOrigin;

    // Extract the manifest.webapp file from application.zip.
    let zipFile = baseDir.clone();
    zipFile.append("application.zip");
    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                      .createInstance(Ci.nsIZipReader);
    try {
      debug("Opening " + zipFile.path);
      zipReader.open(zipFile);
      if (!zipReader.hasEntry("manifest.webapp")) {
        throw "MISSING_MANIFEST";
      }
      let manifestFile = destDir.clone();
      manifestFile.append("manifest.webapp");
      zipReader.extract("manifest.webapp", manifestFile);
    } catch(e) {
      // If we are unable to extract the manifest, cleanup and remove this app.
      debug("Cleaning up: " + e);
      destDir.remove(true);
      delete this.webapps[destId];
    } finally {
      zipReader.close();
    }
    return isPreinstalled;
#endif
  },

  // For hosted apps, uninstall an app served from http:// if we have
  // one installed from the same url with an https:// scheme.
  removeIfHttpsDuplicate: function(aId) {
#ifdef MOZ_WIDGET_GONK
    let app = this.webapps[aId];
    if (!app || !app.origin.startsWith("http://")) {
      return;
    }

    let httpsManifestURL =
      "https://" + app.manifestURL.substring("http://".length);

    // This will uninstall the http apps and remove any data hold by this
    // app. Bug 948105 tracks data migration from http to https apps.
    for (let id in this.webapps) {
       if (this.webapps[id].manifestURL === httpsManifestURL) {
         debug("Found a http/https match: " + app.manifestURL + " / " +
               this.webapps[id].manifestURL);
         this.uninstall(app.manifestURL);
         return;
       }
    }
#endif
  },

  // Implements the core of bug 787439
  // if at first run, go through these steps:
  //   a. load the core apps registry.
  //   b. uninstall any core app from the current registry but not in the
  //      new core apps registry.
  //   c. for all apps in the new core registry, install them if they are not
  //      yet in the current registry, and run installPermissions()
  installSystemApps: function() {
    return Task.spawn(function*() {
      let file;
      try {
        file = FileUtils.getFile("coreAppsDir", ["webapps", "webapps.json"], false);
      } catch(e) { }

      if (!file || !file.exists()) {
        return;
      }

      // a
      let data = yield AppsUtils.loadJSONAsync(file.path);
      if (!data) {
        return;
      }

      // b : core apps are not removable.
      for (let id in this.webapps) {
        if (id in data || this.webapps[id].removable)
          continue;
        // Remove the permissions, cookies and private data for this app.
        let localId = this.webapps[id].localId;
        permMgr.removePermissionsForApp(localId, false);
        Services.cookies.removeCookiesForApp(localId, false);
        this._clearPrivateData(localId, false);
        delete this.webapps[id];
      }

      let appDir = FileUtils.getDir("coreAppsDir", ["webapps"], false);
      // c
      for (let id in data) {
        // Core apps have ids matching their domain name (eg: dialer.gaiamobile.org)
        // Use that property to check if they are new or not.
        // Note that in some cases, the id might change, but the
        // manifest URL wont. So consider that the app is old if
        // the id does not exist already and if there's no other id
        // for the manifestURL.
        var oldId = (id in this.webapps) ? id :
                      this._appIdForManifestURL(data[id].manifestURL);
        if (!oldId) {
          this.webapps[id] = data[id];
          this.webapps[id].basePath = appDir.path;

          this.webapps[id].id = id;

          // Create a new localId.
          this.webapps[id].localId = this._nextLocalId();

          // Core apps are not removable.
          if (this.webapps[id].removable === undefined) {
            this.webapps[id].removable = false;
          }
        } else {
          // Fields that we must not update. Confere bug 993011 comment 10.
          let fieldsBlacklist = ["basePath", "id", "installerAppId",
            "installerIsBrowser", "localId", "receipts", "storeId",
            "storeVersion"];
          // we fall into this case if the app is present in /system/b2g/webapps/webapps.json
          // and in /data/local/webapps/webapps.json: this happens when updating gaia apps
          // Confere bug 989876
          // We also should fall in this case when the app is a preinstalled third party app.
          for (let field in data[id]) {
            if (fieldsBlacklist.indexOf(field) === -1) {
              this.webapps[oldId][field] = data[id][field];
            }
          }
          // If the id for the app has changed on the update, keep a pointer to the old one
          // since we'll need this to update the app files.
          if (id !== oldId) {
            this.webapps[id] = {oldId: oldId};
          }
        }
      }
    }.bind(this)).then(null, Cu.reportError);
  },

  loadAndUpdateApps: function() {
    return Task.spawn(function*() {
      let runUpdate = AppsUtils.isFirstRun(Services.prefs);
      let loadAppPermission = Services.prefs.getBoolPref("dom.apps.reset-permissions");

      yield this.loadCurrentRegistry();

      // Sanity check and roll back previous incomplete app updates.
      for (let id in this.webapps) {
        let oldDir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", id + ".old"], false, true);
        if (oldDir.exists()) {
          let dir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", id], false, true);
          if (dir.exists()) {
            dir.remove(true);
          }
          oldDir.moveTo(null, id);
        }
      }

      try {
        let systemManifestURL =
          Services.prefs.getCharPref("b2g.system_manifest_url");
        let systemAppFound =
          this.webapps.some(v => v.manifestURL == systemManifestURL);

        // We configured a system app but can't find it. That prevents us
        // from starting so we clear our registry to start again from scratch.
        if (!systemAppFound) {
          runUpdate = true;
        }
      } catch(e) {} // getCharPref will throw on non-b2g platforms. That's ok.

      if (runUpdate || !loadAppPermission) {

        // Run migration before uninstall of core apps happens.
        let appMigrator = Components.classes["@mozilla.org/app-migrator;1"];
        if (appMigrator) {
          try {
              appMigrator = appMigrator.createInstance(Components.interfaces.nsIObserver);
              appMigrator.observe(null, "webapps-before-update-merge", null);
          } catch(e) {
            debug("Exception running app migration: ");
            debug(e.name + " " + e.message);
            debug("Skipping app migration.");
          }
        }

#ifdef MOZ_B2G
        yield this.installSystemApps();
#endif

        // At first run, install preloaded apps and set up their permissions.
        for (let id in this.webapps) {
          let isPreinstalled = this.installPreinstalledApp(id);
          this.removeIfHttpsDuplicate(id);
          if (!this.webapps[id]) {
            continue;
          }
          this.updateOfflineCacheForApp(id);
          this.updatePermissionsForApp(id, isPreinstalled);
        }
        // Need to update the persisted list of apps since
        // installPreinstalledApp() removes the ones failing to install.
        this._saveApps();

	Services.prefs.setBoolPref("dom.apps.reset-permissions", true);
      }

      // DataStores must be initialized at startup.
      for (let id in this.webapps) {
        yield this.updateDataStoreForApp(id);
      }

      yield this.registerAppsHandlers(runUpdate);
    }.bind(this)).then(null, Cu.reportError);
  },

  updateDataStore: function(aId, aOrigin, aManifestURL, aManifest) {
    if (!aManifest) {
      debug("updateDataStore: no manifest for " + aOrigin);
      return;
    }

    let uri = Services.io.newURI(aOrigin, null, null);
    let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"]
                   .getService(Ci.nsIScriptSecurityManager);
    let principal = secMan.getAppCodebasePrincipal(uri, aId,
                                                   /*mozbrowser*/ false);
    if (!dataStoreService.checkPermission(principal)) {
      return;
    }

    if ('datastores-owned' in aManifest) {
      for (let name in aManifest['datastores-owned']) {
        let readonly = "access" in aManifest['datastores-owned'][name]
                         ? aManifest['datastores-owned'][name].access == 'readonly'
                         : false;

        dataStoreService.installDataStore(aId, name, aOrigin, aManifestURL,
                                          readonly);
      }
    }

    if ('datastores-access' in aManifest) {
      for (let name in aManifest['datastores-access']) {
        let readonly = ("readonly" in aManifest['datastores-access'][name]) &&
                       !aManifest['datastores-access'][name].readonly
                         ? false : true;

        dataStoreService.installAccessDataStore(aId, name, aOrigin,
                                                aManifestURL, readonly);
      }
    }
  },

  // |aEntryPoint| is either the entry_point name or the null in which case we
  // use the root of the manifest.
  //
  // TODO Bug 908094 Refine _registerSystemMessagesForEntryPoint(...).
  _registerSystemMessagesForEntryPoint: function(aManifest, aApp, aEntryPoint) {
    let root = aManifest;
    if (aEntryPoint && aManifest.entry_points[aEntryPoint]) {
      root = aManifest.entry_points[aEntryPoint];
    }

    if (!root.messages || !Array.isArray(root.messages) ||
        root.messages.length == 0) {
      return;
    }

    let manifest = new ManifestHelper(aManifest, aApp.origin, aApp.manifestURL);
    let launchPathURI = Services.io.newURI(manifest.fullLaunchPath(aEntryPoint), null, null);
    let manifestURI = Services.io.newURI(aApp.manifestURL, null, null);
    root.messages.forEach(function registerPages(aMessage) {
      let handlerPageURI = launchPathURI;
      let messageName;
      if (typeof(aMessage) === "object" && Object.keys(aMessage).length === 1) {
        messageName = Object.keys(aMessage)[0];
        let handlerPath = aMessage[messageName];
        // Resolve the handler path from origin. If |handler_path| is absent,
        // simply skip.
        let fullHandlerPath;
        try {
          if (handlerPath && handlerPath.trim()) {
            fullHandlerPath = manifest.resolveURL(handlerPath);
          } else {
            throw new Error("Empty or blank handler path.");
          }
        } catch(e) {
          debug("system message handler path (" + handlerPath + ") is " +
                "invalid, skipping. Error is: " + e);
          return;
        }
        handlerPageURI = Services.io.newURI(fullHandlerPath, null, null);
      } else {
        messageName = aMessage;
      }

      if (SystemMessagePermissionsChecker
            .isSystemMessagePermittedToRegister(messageName,
                                                aApp.manifestURL,
                                                aManifest)) {
        msgmgr.registerPage(messageName, handlerPageURI, manifestURI);
      }
    });
  },

  // |aEntryPoint| is either the entry_point name or the null in which case we
  // use the root of the manifest.
  //
  // TODO Bug 908094 Refine _registerInterAppConnectionsForEntryPoint(...).
  _registerInterAppConnectionsForEntryPoint: function(aManifest, aApp,
                                                      aEntryPoint) {
    let root = aManifest;
    if (aEntryPoint && aManifest.entry_points[aEntryPoint]) {
      root = aManifest.entry_points[aEntryPoint];
    }

    let connections = root.connections;
    if (!connections) {
      return;
    }

    if ((typeof connections) !== "object") {
      debug("|connections| is not an object. Skipping: " + connections);
      return;
    }

    let manifest = new ManifestHelper(aManifest, aApp.origin, aApp.manifestURL);
    let launchPathURI = Services.io.newURI(manifest.fullLaunchPath(aEntryPoint),
                                           null, null);
    let manifestURI = Services.io.newURI(aApp.manifestURL, null, null);

    for (let keyword in connections) {
      let connection = connections[keyword];

      // Resolve the handler path from origin. If |handler_path| is absent,
      // use |launch_path| as default.
      let fullHandlerPath;
      let handlerPath = connection.handler_path;
      if (handlerPath) {
        try {
          fullHandlerPath = manifest.resolveURL(handlerPath);
        } catch(e) {
          debug("Connection's handler path is invalid. Skipping: keyword: " +
                keyword + " handler_path: " + handlerPath);
          continue;
        }
      }
      let handlerPageURI = fullHandlerPath
                           ? Services.io.newURI(fullHandlerPath, null, null)
                           : launchPathURI;

      if (SystemMessagePermissionsChecker
            .isSystemMessagePermittedToRegister("connection",
                                                aApp.manifestURL,
                                                aManifest)) {
        msgmgr.registerPage("connection", handlerPageURI, manifestURI);
      }

      interAppCommService.
        registerConnection(keyword,
                           handlerPageURI,
                           manifestURI,
                           connection.description,
                           connection.rules);
    }
  },

  _registerSystemMessages: function(aManifest, aApp) {
    this._registerSystemMessagesForEntryPoint(aManifest, aApp, null);

    if (!aManifest.entry_points) {
      return;
    }

    for (let entryPoint in aManifest.entry_points) {
      this._registerSystemMessagesForEntryPoint(aManifest, aApp, entryPoint);
    }
  },

  _registerInterAppConnections: function(aManifest, aApp) {
    this._registerInterAppConnectionsForEntryPoint(aManifest, aApp, null);

    if (!aManifest.entry_points) {
      return;
    }

    for (let entryPoint in aManifest.entry_points) {
      this._registerInterAppConnectionsForEntryPoint(aManifest, aApp,
                                                     entryPoint);
    }
  },

  // |aEntryPoint| is either the entry_point name or the null in which case we
  // use the root of the manifest.
  _createActivitiesToRegister: function(aManifest, aApp, aEntryPoint, aRunUpdate) {
    let activitiesToRegister = [];
    let root = aManifest;
    if (aEntryPoint && aManifest.entry_points[aEntryPoint]) {
      root = aManifest.entry_points[aEntryPoint];
    }

    if (!root || !root.activities) {
      return activitiesToRegister;
    }

    let manifest = new ManifestHelper(aManifest, aApp.origin, aApp.manifestURL);
    for (let activity in root.activities) {
      let description = root.activities[activity];
      let href = description.href;
      if (!href) {
        href = manifest.launch_path;
      }

      try {
        href = manifest.resolveURL(href);
      } catch (e) {
        debug("Activity href (" + href + ") is invalid, skipping. " +
              "Error is: " + e);
        continue;
      }

      // Make a copy of the description object since we don't want to modify
      // the manifest itself, but need to register with a resolved URI.
      let newDesc = {};
      for (let prop in description) {
        newDesc[prop] = description[prop];
      }
      newDesc.href = href;

      debug('_createActivitiesToRegister: ' + aApp.manifestURL + ', activity ' +
          activity + ', description.href is ' + newDesc.href);

      if (aRunUpdate) {
        activitiesToRegister.push({ "manifest": aApp.manifestURL,
                                    "name": activity,
                                    "icon": manifest.iconURLForSize(128),
                                    "description": newDesc });
      }

      let launchPathURI = Services.io.newURI(href, null, null);
      let manifestURI = Services.io.newURI(aApp.manifestURL, null, null);

      if (SystemMessagePermissionsChecker
            .isSystemMessagePermittedToRegister("activity",
                                                aApp.manifestURL,
                                                aManifest)) {
        msgmgr.registerPage("activity", launchPathURI, manifestURI);
      }
    }
    return activitiesToRegister;
  },

  // |aAppsToRegister| contains an array of apps to be registered, where
  // each element is an object in the format of {manifest: foo, app: bar}.
  _registerActivitiesForApps: function(aAppsToRegister, aRunUpdate) {
    // Collect the activities to be registered for root and entry_points.
    let activitiesToRegister = [];
    aAppsToRegister.forEach(function (aApp) {
      let manifest = aApp.manifest;
      let app = aApp.app;
      activitiesToRegister.push.apply(activitiesToRegister,
        this._createActivitiesToRegister(manifest, app, null, aRunUpdate));

      if (aRunUpdate) {
        cpmm.sendAsyncMessage("Activities:UnregisterAll", app.manifestURL);
      }

      if (!manifest.entry_points) {
        return;
      }

      for (let entryPoint in manifest.entry_points) {
        activitiesToRegister.push.apply(activitiesToRegister,
          this._createActivitiesToRegister(manifest, app, entryPoint, aRunUpdate));
      }
    }, this);

    if (!aRunUpdate || activitiesToRegister.length == 0) {
      this.notifyAppsRegistryReady();
      return;
    }

    // Send the array carrying all the activities to be registered.
    cpmm.sendAsyncMessage("Activities:Register", activitiesToRegister);
  },

  // Better to directly use |_registerActivitiesForApps()| if we have
  // multiple apps to be registered for activities.
  _registerActivities: function(aManifest, aApp, aRunUpdate) {
    this._registerActivitiesForApps([{ manifest: aManifest, app: aApp }], aRunUpdate);
  },

  // |aEntryPoint| is either the entry_point name or the null in which case we
  // use the root of the manifest.
  _createActivitiesToUnregister: function(aManifest, aApp, aEntryPoint) {
    let activitiesToUnregister = [];
    let root = aManifest;
    if (aEntryPoint && aManifest.entry_points[aEntryPoint]) {
      root = aManifest.entry_points[aEntryPoint];
    }

    if (!root.activities) {
      return activitiesToUnregister;
    }

    for (let activity in root.activities) {
      let description = root.activities[activity];
      activitiesToUnregister.push({ "manifest": aApp.manifestURL,
                                    "name": activity,
                                    "description": description });
    }
    return activitiesToUnregister;
  },

  // |aAppsToUnregister| contains an array of apps to be unregistered, where
  // each element is an object in the format of {manifest: foo, app: bar}.
  _unregisterActivitiesForApps: function(aAppsToUnregister) {
    // Collect the activities to be unregistered for root and entry_points.
    let activitiesToUnregister = [];
    aAppsToUnregister.forEach(function (aApp) {
      let manifest = aApp.manifest;
      let app = aApp.app;
      activitiesToUnregister.push.apply(activitiesToUnregister,
        this._createActivitiesToUnregister(manifest, app, null));

      if (!manifest.entry_points) {
        return;
      }

      for (let entryPoint in manifest.entry_points) {
        activitiesToUnregister.push.apply(activitiesToUnregister,
          this._createActivitiesToUnregister(manifest, app, entryPoint));
      }
    }, this);

    // Send the array carrying all the activities to be unregistered.
    cpmm.sendAsyncMessage("Activities:Unregister", activitiesToUnregister);
  },

  // Better to directly use |_unregisterActivitiesForApps()| if we have
  // multiple apps to be unregistered for activities.
  _unregisterActivities: function(aManifest, aApp) {
    this._unregisterActivitiesForApps([{ manifest: aManifest, app: aApp }]);
  },

  _processManifestForIds: function(aIds, aRunUpdate) {
    this._readManifests(aIds).then((aResults) => {
      let appsToRegister = [];
      aResults.forEach((aResult) => {
        let app = this.webapps[aResult.id];
        let manifest = aResult.manifest;
        if (!manifest) {
          // If we can't load the manifest, we probably have a corrupted
          // registry. We delete the app since we can't do anything with it.
          delete this.webapps[aResult.id];
          return;
        }

        let localeManifest =
          new ManifestHelper(manifest, app.origin, app.manifestURL);

        app.name = manifest.name;
        app.csp = manifest.csp || "";
        app.role = localeManifest.role;
        this._saveWidgetsFullPath(localeManifest, app);

        if (app.appStatus >= Ci.nsIPrincipal.APP_STATUS_PRIVILEGED) {
          app.redirects = this.sanitizeRedirects(manifest.redirects);
        }
        app.kind = this.appKind(app, aResult.manifest);
        this._registerSystemMessages(manifest, app);
        this._registerInterAppConnections(manifest, app);
        appsToRegister.push({ manifest: manifest, app: app });
        UserCustomizations.register(manifest, app);
        Langpacks.register(app, manifest);
      });
      this._safeToClone.resolve();
      this._registerActivitiesForApps(appsToRegister, aRunUpdate);
    });
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      this.messages.forEach((function(msgName) {
        ppmm.removeMessageListener(msgName, this);
      }).bind(this));
      Services.obs.removeObserver(this, "xpcom-shutdown");
      cpmm = null;
      ppmm = null;
    } else if (aTopic == "memory-pressure") {
      // Clear the manifest cache on memory pressure.
      this._manifestCache = {};
    }
  },

  addMessageListener: function(aMsgNames, aApp, aMm) {
    aMsgNames.forEach(function (aMsgName) {
      let man = aApp && aApp.manifestURL;
      if (!(aMsgName in this.children)) {
        this.children[aMsgName] = [];
      }

      let mmFound = this.children[aMsgName].some(function(mmRef) {
        if (mmRef.mm === aMm) {
          mmRef.refCount++;
          return true;
        }
        return false;
      });

      if (!mmFound) {
        this.children[aMsgName].push({
          mm: aMm,
          refCount: 1
        });
      }

      // If the state reported by the registration is outdated, update it now.
      if ((aMsgName === 'Webapps:FireEvent') ||
          (aMsgName === 'Webapps:UpdateState')) {
        if (man) {
          let app = this.getAppByManifestURL(aApp.manifestURL);
          if (app && ((aApp.installState !== app.installState) ||
                      (aApp.downloading !== app.downloading))) {
            debug("Got a registration from an outdated app: " +
                  aApp.manifestURL);
            let aEvent ={
              type: app.installState,
              app: app,
              manifestURL: app.manifestURL,
              manifest: app.manifest
            };
            aMm.sendAsyncMessage(aMsgName, aEvent);
          }
        }
      }
    }, this);
  },

  removeMessageListener: function(aMsgNames, aMm) {
    if (aMsgNames.length === 1 &&
        aMsgNames[0] === "Webapps:Internal:AllMessages") {
      for (let msgName in this.children) {
        let msg = this.children[msgName];

        for (let mmI = msg.length - 1; mmI >= 0; mmI -= 1) {
          let mmRef = msg[mmI];
          if (mmRef.mm === aMm) {
            msg.splice(mmI, 1);
          }
        }

        if (msg.length === 0) {
          delete this.children[msgName];
        }
      }
      return;
    }

    aMsgNames.forEach(function(aMsgName) {
      if (!(aMsgName in this.children)) {
        return;
      }

      let removeIndex;
      this.children[aMsgName].some(function(mmRef, index) {
        if (mmRef.mm === aMm) {
          mmRef.refCount--;
          if (mmRef.refCount === 0) {
            removeIndex = index;
          }
          return true;
        }
        return false;
      });

      if (removeIndex) {
        this.children[aMsgName].splice(removeIndex, 1);
      }
    }, this);
  },

  formatMessage: function(aData) {
    let msg = aData;
    delete msg["mm"];
    return msg;
  },

  receiveMessage: function(aMessage) {
    // nsIPrefBranch throws if pref does not exist, faster to simply write
    // the pref instead of first checking if it is false.
    Services.prefs.setBoolPref("dom.mozApps.used", true);

    let msg = aMessage.data || {};
    let mm = aMessage.target;
    msg.mm = mm;

    let principal = aMessage.principal;

    let checkPermission = function(aPermission) {
      if (!permMgr.testPermissionFromPrincipal(principal, aPermission)) {
        debug("mozApps message " + aMessage.name +
              " from a content process with no " + aPermission + " privileges.");
        return false;
      }
      return true;
    };

    // We need to check permissions for calls coming from mozApps.mgmt.

    let allowed = true;
    switch (aMessage.name) {
      case "Webapps:Uninstall":
        let isCurrentHomescreen =
          Services.prefs.prefHasUserValue("dom.mozApps.homescreenURL") &&
          Services.prefs.getCharPref("dom.mozApps.homescreenURL") ==
          appsService.getManifestURLByLocalId(principal.appId);

        allowed = checkPermission("webapps-manage") ||
                  (checkPermission("homescreen-webapps-manage") && isCurrentHomescreen);
        break;

      case "Webapps:GetNotInstalled":
      case "Webapps:ApplyDownload":
      case "Webapps:Import":
      case "Webapps:ExtractManifest":
      case "Webapps:SetEnabled":
        allowed = checkPermission("webapps-manage");
        break;

      case "Webapps:RegisterBEP":
        allowed = checkPermission("browser");
        break;

      default:
        break;
    }

    let processedImmediately = true;

    if (!allowed) {
      mm.killChild();
      return null;
    }

    // There are two kind of messages: the messages that only make sense once the
    // registry is ready, and those that can (or have to) be treated as soon as
    // they're received.
    switch (aMessage.name) {
      case "Activities:Register:KO":
        dump("Activities didn't register correctly!");
      case "Activities:Register:OK":
        // Activities:Register:OK is special because it's one way the registryReady
        // promise can be resolved.
        // XXX: What to do when the activities registration failed? At this point
        // just act as if nothing happened.
        this.notifyAppsRegistryReady();
        break;
      case "Webapps:GetList":
        // GetList is special because it's synchronous. So far so well, it's the
        // only synchronous message, if we get more at some point they should get
        // this treatment also.
        return this.doGetList();
      case "child-process-shutdown":
        this.removeMessageListener(["Webapps:Internal:AllMessages"], mm);
        break;
      case "Webapps:RegisterForMessages":
        this.addMessageListener(msg.messages, msg.app, mm);
        break;
      case "Webapps:UnregisterForMessages":
        this.removeMessageListener(msg, mm);
        break;
      default:
        processedImmediately = false;
    }

    if (processedImmediately) {
      return;
    }

    // For all the rest (asynchronous), we wait till the registry is ready
    // before processing the message.
    this.registryReady.then( () => {
      switch (aMessage.name) {
        case "Webapps:Install": {
#ifdef MOZ_WIDGET_ANDROID
          Services.obs.notifyObservers(mm, "webapps-runtime-install", JSON.stringify(msg));
#else
          this.doInstall(msg, mm);
#endif
          break;
        }
        case "Webapps:GetSelf":
          this.getSelf(msg, mm);
          break;
        case "Webapps:Uninstall":
#ifdef MOZ_WIDGET_ANDROID
          Services.obs.notifyObservers(mm, "webapps-runtime-uninstall", JSON.stringify(msg));
#else
          this.doUninstall(msg, mm);
#endif
          break;
        case "Webapps:Launch":
          this.doLaunch(msg, mm);
          break;
        case "Webapps:LocationChange":
          this.onLocationChange(msg.oid);
          break;
        case "Webapps:CheckInstalled":
          this.checkInstalled(msg, mm);
          break;
        case "Webapps:GetInstalled":
          this.getInstalled(msg, mm);
          break;
        case "Webapps:GetNotInstalled":
          this.getNotInstalled(msg, mm);
          break;
        case "Webapps:InstallPackage": {
#ifdef MOZ_WIDGET_ANDROID
          Services.obs.notifyObservers(mm, "webapps-runtime-install-package", JSON.stringify(msg));
#else
          this.doInstallPackage(msg, mm);
#endif
          break;
        }
        case "Webapps:Download":
          this.startDownload(msg.manifestURL);
          break;
        case "Webapps:CancelDownload":
          this.cancelDownload(msg.manifestURL);
          break;
        case "Webapps:CheckForUpdate":
          this.checkForUpdate(msg, mm);
          break;
        case "Webapps:ApplyDownload":
          this.applyDownload(msg.manifestURL);
          break;
        case "Webapps:Install:Return:Ack":
          this.onInstallSuccessAck(msg.manifestURL);
          break;
        case "Webapps:AddReceipt":
          this.addReceipt(msg, mm);
          break;
        case "Webapps:RemoveReceipt":
          this.removeReceipt(msg, mm);
          break;
        case "Webapps:ReplaceReceipt":
          this.replaceReceipt(msg, mm);
          break;
        case "Webapps:RegisterBEP":
          this.registerBrowserElementParentForApp(msg, mm);
          break;
        case "Webapps:GetIcon":
          this.getIcon(msg, mm);
          break;
        case "Webapps:Export":
          this.doExport(msg, mm);
          break;
        case "Webapps:Import":
          this.doImport(msg, mm);
          break;
        case "Webapps:ExtractManifest":
          this.doExtractManifest(msg, mm);
          break;
        case "Webapps:SetEnabled":
          this.setEnabled(msg);
          break;
      }
    });
  },

  getAppInfo: function getAppInfo(aAppId) {
    return AppsUtils.getAppInfo(this.webapps, aAppId);
  },

  // Some messages can be listened by several content processes:
  // Webapps:AddApp
  // Webapps:RemoveApp
  // Webapps:Install:Return:OK
  // Webapps:Uninstall:Return:OK
  // Webapps:Uninstall:Broadcast:Return:OK
  // Webapps:FireEvent
  // Webapps:checkForUpdate:Return:OK
  // Webapps:UpdateState
  broadcastMessage: function broadcastMessage(aMsgName, aContent) {
    if (!(aMsgName in this.children)) {
      return;
    }
    this.children[aMsgName].forEach((mmRef) => {
      mmRef.mm.sendAsyncMessage(aMsgName, this.formatMessage(aContent));
    });
  },

  registerUpdateHandler: function(aHandler) {
    this._updateHandlers.push(aHandler);
  },

  unregisterUpdateHandler: function(aHandler) {
    let index = this._updateHandlers.indexOf(aHandler);
    if (index != -1) {
      this._updateHandlers.splice(index, 1);
    }
  },

  notifyUpdateHandlers: function(aApp, aManifest, aZipPath) {
    for (let updateHandler of this._updateHandlers) {
      updateHandler(aApp, aManifest, aZipPath);
    }
  },

  _getAppDir: function(aId) {
    return FileUtils.getDir(DIRECTORY_NAME, ["webapps", aId], true, true);
  },

  _writeFile: function(aPath, aData) {
    debug("Saving " + aPath);

    let deferred = Promise.defer();

    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(aPath);

    // Initialize the file output stream
    let ostream = FileUtils.openSafeFileOutputStream(file);

    // Obtain a converter to convert our data to a UTF-8 encoded input stream.
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                      .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    // Asynchronously copy the data to the file.
    let istream = converter.convertToInputStream(aData);
    NetUtil.asyncCopy(istream, ostream, function(aResult) {
      if (!Components.isSuccessCode(aResult)) {
        debug("Error saving " + aPath + " : " + aResult);
        deferred.reject(aResult)
      } else {
        debug("Success saving " + aPath);
        deferred.resolve();
      }
    });

    return deferred.promise;
  },

  /**
    * Returns the full list of apps and manifests.
    */
  doGetList: function() {
    let tmp = [];

    let res = {};
    let done = false;

    // We allow cloning the registry when the local processing has been done.
    this.safeToClone.then( () => {
      for (let id in this.webapps) {
        tmp.push({ id: id });
        this.webapps[id].additionalLanguages =
          Langpacks.getAdditionalLanguages(this.webapps[id].manifestURL).langs;
      }
      this._readManifests(tmp).then(
        function(manifests) {
          manifests.forEach((item) => {
            res[item.id] = item.manifest;
          });
          done = true;
        }
      );
    });

    let thread = Services.tm.currentThread;
    while (!done) {
      thread.processNextEvent(/* mayWait */ true);
    }
    return { webapps: this.webapps, manifests: res };
  },

  doExport: function(aMsg, aMm) {

    let sendError = (aError) => {
      aMm.sendAsyncMessage("Webapps:Export:Return",
        { requestID: aMsg.requestID,
          oid: aMsg.oid,
          error: aError,
          success: false
        });
    };

    let app = this.getAppByManifestURL(aMsg.manifestURL);
    if (!app) {
      sendError("NoSuchApp");
      return;
    }

    ImportExport.export(app).then(
      aBlob => {
        debug("exporting " + aBlob);
        aMm.sendAsyncMessage("Webapps:Export:Return",
          { requestID: aMsg.requestID,
            oid: aMsg.oid,
            blob: aBlob,
            success: true
          });
      },
      aError => sendError(aError));
  },

  doImport: function(aMsg, aMm) {
    let sendError = (aError) => {
      aMm.sendAsyncMessage("Webapps:Import:Return",
        { requestID: aMsg.requestID,
          oid: aMsg.oid,
          error: aError,
          success: false
        });
    };

    if (!aMsg.blob || !aMsg.blob instanceof Ci.nsIDOMBlob) {
      sendError("NoBlobFound");
      return;
    }

    ImportExport.import(aMsg.blob).then(
      ([aManifestURL, aManifest]) => {
        let app = this.getAppByManifestURL(aManifestURL);
        app.manifest = aManifest;
        aMm.sendAsyncMessage("Webapps:Import:Return",
          { requestID: aMsg.requestID,
            oid: aMsg.oid,
            app: app,
            success: true
          });
      },
      aError => sendError(aError));
  },

  doExtractManifest: function(aMsg, aMm) {
    let sendError = (aError) => {
      aMm.sendAsyncMessage("Webapps:ExtractManifest:Return",
        { requestID: aMsg.requestID,
          oid: aMsg.oid,
          error: aError,
          success: false
        });
    };

    if (!aMsg.blob || !aMsg.blob instanceof Ci.nsIDOMBlob) {
      sendError("NoBlobFound");
      return;
    }

    ImportExport.extractManifest(aMsg.blob).then(
      aManifest => {
        aMm.sendAsyncMessage("Webapps:ExtractManifest:Return",
          { requestID: aMsg.requestID,
            oid: aMsg.oid,
            manifest: aManifest,
            success: true
          });
      },
      aError => {
        aMm.sendAsyncMessage("Webapps:ExtractManifest:Return",
          { requestID: aMsg.requestID,
            oid: aMsg.oid,
            error: aError,
            success: false
          });
      }
    );
  },

  doLaunch: function (aData, aMm) {
    this.launch(
      aData.manifestURL,
      aData.startPoint,
      aData.timestamp,
      () => {
        aMm.sendAsyncMessage("Webapps:Launch:Return:OK", this.formatMessage(aData));
      },
      (reason) => {
        aData.error = reason;
        aMm.sendAsyncMessage("Webapps:Launch:Return:KO", this.formatMessage(aData));
      }
    );
  },

  launch: function launch(aManifestURL, aStartPoint, aTimeStamp, aOnSuccess, aOnFailure) {
    let app = this.getAppByManifestURL(aManifestURL);
    if (!app) {
      aOnFailure("NO_SUCH_APP");
      return;
    }

    // Fire an error when trying to launch an app that is not
    // yet fully installed.
    if (app.installState == "pending") {
      aOnFailure("PENDING_APP_NOT_LAUNCHABLE");
      return;
    }

    // Check if launching trusted hosted app
    if (this.kTrustedHosted == app.kind) {
      debug("Launching Trusted Hosted App!");
      // sanity check on manifest host's CA
      // (proper CA check with pinning is done by regular networking code)
      if (!TrustedHostedAppsUtils.isHostPinned(aManifestURL)) {
        debug("Trusted App Host certificate Not OK");
        aOnFailure("TRUSTED_APPLICATION_HOST_CERTIFICATE_INVALID");
        return;
      }

      debug("Trusted App Host pins exist");
      if (!TrustedHostedAppsUtils.verifyCSPWhiteList(app.csp)) {
        aOnFailure("TRUSTED_APPLICATION_WHITELIST_VALIDATION_FAILED");
        return;
      }
    }

    // We have to clone the app object as nsIDOMApplication objects are
    // stringified as an empty object. (see bug 830376)
    let appClone = AppsUtils.cloneAppObject(app);
    appClone.startPoint = aStartPoint;
    appClone.timestamp = aTimeStamp;
    Services.obs.notifyObservers(null, "webapps-launch", JSON.stringify(appClone));
    aOnSuccess();
  },

  close: function close(aApp) {
    debug("close");

    // We have to clone the app object as nsIDOMApplication objects are
    // stringified as an empty object. (see bug 830376)
    let appClone = AppsUtils.cloneAppObject(aApp);
    Services.obs.notifyObservers(null, "webapps-close", JSON.stringify(appClone));
  },

  cancelDownload: function cancelDownload(aManifestURL, aError) {
    debug("cancelDownload " + aManifestURL);
    let error = aError || "DOWNLOAD_CANCELED";
    let download = AppDownloadManager.get(aManifestURL);
    if (!download) {
      debug("Could not find a download for " + aManifestURL);
      return;
    }

    let app = this.webapps[download.appId];

    if (download.cacheUpdate) {
      try {
        download.cacheUpdate.cancel();
      } catch (e) {
        debug (e);
      }
    } else if (download.channel) {
      try {
        download.channel.cancel(Cr.NS_BINDING_ABORTED);
      } catch(e) { }
    } else {
      return;
    }

    // Ensure we don't send additional errors for this download
    app.isCanceling = true;

    // Ensure this app can be downloaded again after canceling
    app.downloading = false;

    this._saveApps().then(() => {
      this.broadcastMessage("Webapps:UpdateState", {
        app: {
          progress: 0,
          installState: download.previousState,
          downloading: false
        },
        error: error,
        id: app.id
      })
      this.broadcastMessage("Webapps:FireEvent", {
        eventType: "downloaderror",
        manifestURL: app.manifestURL
      });
    });
    AppDownloadManager.remove(aManifestURL);
  },

  startDownload: Task.async(function*(aManifestURL) {
    debug("startDownload for " + aManifestURL);

    let id = this._appIdForManifestURL(aManifestURL);
    let app = this.webapps[id];

    if (!app) {
      debug("startDownload: No app found for " + aManifestURL);
      throw new Error("NO_SUCH_APP");
    }

    if (app.downloading) {
      debug("app is already downloading. Ignoring.");
      throw new Error("APP_IS_DOWNLOADING");
    }

    // If the caller is trying to start a download but we have nothing to
    // download, send an error.
    if (!app.downloadAvailable) {
      this.broadcastMessage("Webapps:UpdateState", {
        error: "NO_DOWNLOAD_AVAILABLE",
        id: app.id
      });
      this.broadcastMessage("Webapps:FireEvent", {
        eventType: "downloaderror",
        manifestURL: app.manifestURL
      });
      throw new Error("NO_DOWNLOAD_AVAILABLE");
    }

    // First of all, we check if the download is supposed to update an
    // already installed application.
    let isUpdate = (app.installState == "installed");

    // An app download would only be triggered for two reasons: an app
    // update or while retrying to download a previously failed or canceled
    // instalation.
    app.retryingDownload = !isUpdate;

    // We need to get the update manifest here, not the webapp manifest.
    // If this is an update, the update manifest is staged.
    let file = FileUtils.getFile(DIRECTORY_NAME,
                                 ["webapps", id,
                                  isUpdate ? "staged-update.webapp"
                                           : "update.webapp"],
                                 true);

    if (!file.exists()) {
      // This is a hosted app, let's check if it has an appcache
      // and download it.
      let results = yield this._readManifests([{ id: id }]);

      let jsonManifest = results[0].manifest;
      let manifest =
        new ManifestHelper(jsonManifest, app.origin, app.manifestURL);

      if (manifest.appcache_path) {
        debug("appcache found");
        this.startOfflineCacheDownload(manifest, app, null, isUpdate);
      } else {
        // Hosted app with no appcache, nothing to do, but we fire a
        // downloaded event.
        debug("No appcache found, sending 'downloaded' for " + aManifestURL);
        app.downloadAvailable = false;

        yield this._saveApps();

        this.broadcastMessage("Webapps:UpdateState", {
          app: app,
          manifest: jsonManifest,
          id: app.id
        });
        this.broadcastMessage("Webapps:FireEvent", {
          eventType: "downloadsuccess",
          manifestURL: aManifestURL
        });
      }

      return;
    }

    let json = yield AppsUtils.loadJSONAsync(file.path);
    if (!json) {
      debug("startDownload: No update manifest found at " + file.path + " " +
            aManifestURL);
      throw new Error("MISSING_UPDATE_MANIFEST");
    }

    let manifest = new ManifestHelper(json, app.origin, app.manifestURL);
    let newApp = {
      manifestURL: aManifestURL,
      origin: app.origin,
      installOrigin: app.installOrigin,
      downloadSize: app.downloadSize
    };

    let newManifest, newId;

    try {
      [newId, newManifest] = yield this.downloadPackage(id, app, manifest, newApp, isUpdate);
    } catch (ex) {
      this.revertDownloadPackage(id, app, newApp, isUpdate, ex);
      throw ex;
    }

    // Success! Keep the zip in of TmpD, we'll move it out when
    // applyDownload() will be called.
    // Save the manifest in TmpD also
    let manFile = OS.Path.join(OS.Constants.Path.tmpDir, "webapps", newId,
                               "manifest.webapp");
    yield this._writeFile(manFile, JSON.stringify(newManifest));

    app = this.webapps[id];

    // Set state and fire events.
    app.downloading = false;
    app.downloadAvailable = false;
    app.readyToApplyDownload = true;
    app.updateTime = Date.now();

    this.broadcastMessage("Webapps:UpdateState", {
      app: app,
      id: app.id
    });
    this.broadcastMessage("Webapps:FireEvent", {
      eventType: "downloadsuccess",
      manifestURL: aManifestURL
    });
    if (app.installState == "pending") {
      // We restarted a failed download, apply it automatically.
      this.applyDownload(aManifestURL);
    }
  }),

  applyDownload: Task.async(function*(aManifestURL) {
    debug("applyDownload for " + aManifestURL);
    let id = this._appIdForManifestURL(aManifestURL);
    let app = this.webapps[id];
    if (!app) {
      throw new Error("NO_SUCH_APP");
    }
    if (!app.readyToApplyDownload) {
      throw new Error("NOT_READY_TO_APPLY_DOWNLOAD");
    }

    // We need to get the old manifest to unregister web activities.
    let oldManifest = yield this.getManifestFor(aManifestURL);
    // Move the application.zip and manifest.webapp files out of TmpD
    let tmpDir = FileUtils.getDir("TmpD", ["webapps", id], true, true);
    let manFile = tmpDir.clone();
    manFile.append("manifest.webapp");
    let appFile = tmpDir.clone();
    appFile.append("application.zip");

    // In order to better control the potential inconsistency due to unexpected
    // shutdown during the update process, a separate folder is used to accommodate
    // the updated files and to replace the current one. Some sanity check and
    // correspondent rollback logic may be necessary during the initialization
    // of this component to recover it at next system boot-up.
    let oldDir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", id], true, true);
    let dir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", id + ".new"], true, true);
    appFile.moveTo(dir, "application.zip");
    manFile.moveTo(dir, "manifest.webapp");

    // Copy the staged update manifest to a non staged one.
    let staged = oldDir.clone();
    staged.append("staged-update.webapp");

    // If we are applying after a restarted download, we have no
    // staged update manifest.
    if (staged.exists()) {
      staged.copyTo(dir, "update.webapp");
    }

    oldDir.moveTo(null, id + ".old");
    dir.moveTo(null, id);

    try {
      oldDir.remove(true);
    } catch(e) {
      oldDir.moveTo(tmpDir, "old." + app.updateTime);
    }

    try {
      tmpDir.remove(true);
    } catch(e) { }

    // Clean up the deprecated manifest cache if needed.
    if (id in this._manifestCache) {
      delete this._manifestCache[id];
    }

    // Flush the zip reader cache to make sure we use the new application.zip
    // when re-launching the application.
    let zipFile = dir.clone();
    zipFile.append("application.zip");
    Services.obs.notifyObservers(zipFile, "flush-cache-entry", null);

    // Get the manifest, and set properties.
    let newManifest = yield this.getManifestFor(aManifestURL);
    app.downloading = false;
    app.downloadAvailable = false;
    app.downloadSize = 0;
    app.installState = "installed";
    app.readyToApplyDownload = false;

    // Update the staged properties.
    if (app.staged) {
      for (let prop in app.staged) {
        app[prop] = app.staged[prop];
      }
      delete app.staged;
    }

    delete app.retryingDownload;

    // Update the asm.js scripts we need to compile.
    yield ScriptPreloader.preload(app, newManifest);

    // Update langpack information.
    Langpacks.register(app, newManifest);

    yield this._saveApps();
    // Update the handlers and permissions for this app.
    this.updateAppHandlers(oldManifest, newManifest, app);

    let updateManifest = yield AppsUtils.loadJSONAsync(staged.path);
    let appObject = AppsUtils.cloneAppObject(app);
    appObject.updateManifest = updateManifest;
    this.notifyUpdateHandlers(appObject, newManifest, appFile.path);

    if (supportUseCurrentProfile()) {
      PermissionsInstaller.installPermissions(
        { manifest: newManifest,
          origin: app.origin,
          manifestURL: app.manifestURL,
          kind: app.kind },
        true);
    }
    this.updateDataStore(this.webapps[id].localId, app.origin,
                         app.manifestURL, newManifest);
    this.broadcastMessage("Webapps:UpdateState", {
      app: app,
      manifest: newManifest,
      id: app.id
    });
    this.broadcastMessage("Webapps:FireEvent", {
      eventType: "downloadapplied",
      manifestURL: app.manifestURL
    });
  }),

  startOfflineCacheDownload: function(aManifest, aApp, aProfileDir, aIsUpdate) {
    debug("startOfflineCacheDownload " + aApp.id + " " + aApp.kind);
    if ((aApp.kind !== this.kHostedAppcache &&
         aApp.kind !== this.kTrustedHosted) ||
         !aManifest.appcache_path) {
      return;
    }
    debug("startOfflineCacheDownload " + aManifest.appcache_path);

    // If the manifest has an appcache_path property, use it to populate the
    // appcache.
    let appcacheURI = Services.io.newURI(aManifest.fullAppcachePath(),
                                         null, null);
    let docURI = Services.io.newURI(aManifest.fullLaunchPath(), null, null);

    // We determine the app's 'installState' according to its previous
    // state. Cancelled downloads should remain as 'pending'. Successfully
    // installed apps should morph to 'updating'.
    if (aIsUpdate) {
      aApp.installState = "updating";
    }

    // We set the 'downloading' flag and update the apps registry right before
    // starting the app download/update.
    aApp.downloading = true;
    aApp.progress = 0;
    DOMApplicationRegistry._saveApps().then(() => {
      DOMApplicationRegistry.broadcastMessage("Webapps:UpdateState", {
        // Clear any previous errors.
        error: null,
        app: {
          downloading: true,
          installState: aApp.installState,
          progress: 0
        },
        id: aApp.id
      });
      let cacheUpdate = updateSvc.scheduleAppUpdate(
        appcacheURI, docURI, aApp.localId, false, aProfileDir);

      // We save the download details for potential further usage like
      // cancelling it.
      let download = {
        cacheUpdate: cacheUpdate,
        appId: this._appIdForManifestURL(aApp.manifestURL),
        previousState: aIsUpdate ? "installed" : "pending"
      };
      AppDownloadManager.add(aApp.manifestURL, download);

      cacheUpdate.addObserver(new AppcacheObserver(aApp), false);

    });
  },

  // Returns the MD5 hash of the manifest.
  computeManifestHash: function(aManifest) {
    return AppsUtils.computeHash(JSON.stringify(aManifest));
  },

  // Updates the redirect mapping, activities and system message handlers.
  // aOldManifest can be null if we don't have any handler to unregister.
  updateAppHandlers: function(aOldManifest, aNewManifest, aApp) {
    debug("updateAppHandlers: old=" + uneval(aOldManifest) +
          " new=" + uneval(aNewManifest));
    this.notifyAppsRegistryStart();
    if (aApp.appStatus >= Ci.nsIPrincipal.APP_STATUS_PRIVILEGED) {
      aApp.redirects = this.sanitizeRedirects(aNewManifest.redirects);
    }

    let manifest =
      new ManifestHelper(aNewManifest, aApp.origin, aApp.manifestURL);
    this._saveWidgetsFullPath(manifest, aApp);

    aApp.role = manifest.role ? manifest.role : "";

    if (supportSystemMessages()) {
      if (aOldManifest) {
        this._unregisterActivities(aOldManifest, aApp);
      }
      this._registerSystemMessages(aNewManifest, aApp);
      this._registerActivities(aNewManifest, aApp, true);
      this._registerInterAppConnections(aNewManifest, aApp);
    } else {
      // Nothing else to do but notifying we're ready.
      this.notifyAppsRegistryReady();
    }

    // Update user customizations and langpacks.
    if (aOldManifest) {
      UserCustomizations.unregister(aOldManifest, aApp);
      Langpacks.unregister(aApp, aOldManifest);
    }
    UserCustomizations.register(aNewManifest, aApp);
    Langpacks.register(aApp, aNewManifest);
  },

  checkForUpdate: function(aData, aMm) {
    debug("checkForUpdate for " + aData.manifestURL);

    let sendError = (aError) => {
      debug("checkForUpdate error " + aError);
      aData.error = aError;
      aMm.sendAsyncMessage("Webapps:CheckForUpdate:Return:KO", this.formatMessage(aData));
    };

    let id = this._appIdForManifestURL(aData.manifestURL);
    let app = this.webapps[id];

    // We cannot update an app that does not exists.
    if (!app) {
      sendError("NO_SUCH_APP");
      return;
    }

    // We cannot update an app that is not fully installed.
    if (app.installState !== "installed") {
      sendError("PENDING_APP_NOT_UPDATABLE");
      return;
    }

    // We may be able to remove this when Bug 839071 is fixed.
    if (app.downloading) {
      sendError("APP_IS_DOWNLOADING");
      return;
    }

    // If the app is packaged and its manifestURL has an app:// scheme,
    // then we can't have an update.
    if (app.kind == this.kPackaged && app.manifestURL.startsWith("app://")) {
      sendError("NOT_UPDATABLE");
      return;
    }

    // For non-removable hosted apps that lives in the core apps dir we
    // only check the appcache because we can't modify the manifest even
    // if it has changed.
    let onlyCheckAppCache = false;

#ifdef MOZ_WIDGET_GONK
    let appDir = FileUtils.getDir("coreAppsDir", ["webapps"], false);
    onlyCheckAppCache = (app.basePath == appDir.path);
#endif

    if (onlyCheckAppCache) {
      // Bail out for packaged apps & hosted apps without appcache.
      if (aApp.kind !== this.kHostedAppcache &&
          aApp.kind !== this.kTrustedHosted) {
        sendError("NOT_UPDATABLE");
        return;
      }

      // We need the manifest to get the appcache path.
      this._readManifests([{ id: id }]).then((aResult) => {
        debug("Checking only appcache for " + aData.manifestURL);
        let manifest = aResult[0].manifest;
        if (!manifest.appcache_path) {
          sendError("NOT_UPDATABLE");
          return;
        }
        // Check if the appcache is updatable, and send "downloadavailable" or
        // "downloadapplied".
        let updateObserver = {
          observe: function(aSubject, aTopic, aObsData) {
            debug("onlyCheckAppCache updateSvc.checkForUpdate return for " +
                  app.manifestURL + " - event is " + aTopic);
            if (aTopic == "offline-cache-update-available") {
              app.downloadAvailable = true;
              this._saveApps().then(() => {
                this.broadcastMessage("Webapps:UpdateState", {
                  app: app,
                  id: app.id
                });
                this.broadcastMessage("Webapps:FireEvent", {
                  eventType: "downloadavailable",
                  manifestURL: app.manifestURL,
                  requestID: aData.requestID
                });
              });
            } else {
              sendError("NOT_UPDATABLE");
            }
          }
        };
        let helper =
          new ManifestHelper(manifest, aData.origin, aData.manifestURL);
        debug("onlyCheckAppCache - launch updateSvc.checkForUpdate for " +
              helper.fullAppcachePath());
        updateSvc.checkForUpdate(Services.io.newURI(helper.fullAppcachePath(), null, null),
                                 app.localId, false, updateObserver);
      });
      return;
    }

    // On xhr load request event
    function onload(xhr, oldManifest) {
      debug("Got http status=" + xhr.status + " for " + aData.manifestURL);
      let oldHash = app.manifestHash;
      let isPackage = app.kind == DOMApplicationRegistry.kPackaged;

      if (xhr.status == 200) {
        let manifest = xhr.response;
        if (manifest == null) {
          sendError("MANIFEST_PARSE_ERROR");
          return;
        }

        if (!AppsUtils.checkManifest(manifest, app)) {
          sendError("INVALID_MANIFEST");
          return;
        } else if (!AppsUtils.checkInstallAllowed(manifest, app.installOrigin)) {
          sendError("INSTALL_FROM_DENIED");
          return;
        } else {
          AppsUtils.ensureSameAppName(oldManifest, manifest, app);

          let hash = this.computeManifestHash(manifest);
          debug("Manifest hash = " + hash);
          if (isPackage) {
            if (!app.staged) {
              app.staged = { };
            }
            app.staged.manifestHash = hash;
            app.staged.etag = xhr.getResponseHeader("Etag");
          } else {
            app.manifestHash = hash;
            app.etag = xhr.getResponseHeader("Etag");
          }

          app.lastCheckedUpdate = Date.now();
          if (isPackage) {
            if (oldHash != hash) {
              this.updatePackagedApp(aData, id, app, manifest);
            } else {
              this._saveApps().then(() => {
                // Like if we got a 304, just send a 'downloadapplied'
                // or downloadavailable event.
                let eventType = app.downloadAvailable ? "downloadavailable"
                                                      : "downloadapplied";
                aMm.sendAsyncMessage("Webapps:UpdateState", {
                  app: app,
                  id: app.id
                });
                aMm.sendAsyncMessage("Webapps:FireEvent", {
                  eventType: eventType,
                  manifestURL: app.manifestURL,
                  requestID: aData.requestID
                });
              });
            }
          } else {
            // Update only the appcache if the manifest has not changed
            // based on the hash value.
            if (oldHash == hash) {
              debug("Update - oldhash");
              this.updateHostedApp(aData, id, app, oldManifest, null);
              return;
            }

            // For hosted apps and hosted apps with appcache, use the
            // manifest "as is".
            if (this.kTrustedHosted !== this.appKind(app, manifest)) {
              this.updateHostedApp(aData, id, app, oldManifest, manifest);
              return;
            }

            // For trusted hosted apps, verify the manifest before
            // installation.
            TrustedHostedAppsUtils.verifyManifest(app, id, manifest)
              .then(() => this.updateHostedApp(aData, id, app, oldManifest, manifest),
                sendError);
          }
        }
      } else if (xhr.status == 304) {
        // The manifest has not changed.
        if (isPackage) {
          app.lastCheckedUpdate = Date.now();
          this._saveApps().then(() => {
            // If the app is a packaged app, we just send a 'downloadapplied'
            // or downloadavailable event.
            let eventType = app.downloadAvailable ? "downloadavailable"
                                                  : "downloadapplied";
            aMm.sendAsyncMessage("Webapps:UpdateState", {
              app: app,
              id: app.id
            });
            aMm.sendAsyncMessage("Webapps:FireEvent", {
              eventType: eventType,
              manifestURL: app.manifestURL,
              requestID: aData.requestID
            });
          });
        } else {
          // For hosted apps, even if the manifest has not changed, we check
          // for offline cache updates.
          this.updateHostedApp(aData, id, app, oldManifest, null);
        }
      } else {
        sendError("MANIFEST_URL_ERROR");
      }
    }

    // Try to download a new manifest.
    function doRequest(oldManifest, headers) {
      headers = headers || [];
      let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                  .createInstance(Ci.nsIXMLHttpRequest);
      xhr.open("GET", aData.manifestURL, true);
      xhr.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
      headers.forEach(function(aHeader) {
        debug("Adding header: " + aHeader.name + ": " + aHeader.value);
        xhr.setRequestHeader(aHeader.name, aHeader.value);
      });
      xhr.responseType = "json";
      if (app.etag) {
        debug("adding manifest etag:" + app.etag);
        xhr.setRequestHeader("If-None-Match", app.etag);
      }
      xhr.channel.notificationCallbacks =
        AppsUtils.createLoadContext(app.installerAppId, app.installerIsBrowser);

      xhr.addEventListener("load", onload.bind(this, xhr, oldManifest), false);
      xhr.addEventListener("error", (function() {
        sendError("NETWORK_ERROR");
      }).bind(this), false);

      debug("Checking manifest at " + aData.manifestURL);
      xhr.send(null);
    }

    // Read the current app manifest file
    this._readManifests([{ id: id }]).then((aResult) => {
      let extraHeaders = [];
#ifdef MOZ_WIDGET_GONK
      let pingManifestURL;
      try {
        pingManifestURL = Services.prefs.getCharPref("ping.manifestURL");
      } catch(e) { }

      if (pingManifestURL && pingManifestURL == aData.manifestURL) {
        // Get the device info.
        let device = libcutils.property_get("ro.product.model");
        extraHeaders.push({ name: "X-MOZ-B2G-DEVICE",
                            value: device || "unknown" });
      }
#endif
      doRequest.call(this, aResult[0].manifest, extraHeaders);
    });
  },

  updatePackagedApp: Task.async(function*(aData, aId, aApp, aNewManifest) {
    debug("updatePackagedApp");

    // Store the new update manifest.
    let dir = this._getAppDir(aId).path;
    let manFile = OS.Path.join(dir, "staged-update.webapp");
    yield this._writeFile(manFile, JSON.stringify(aNewManifest));

    let manifest =
      new ManifestHelper(aNewManifest, aApp.origin, aApp.manifestURL);
    // A package is available: set downloadAvailable to fire the matching
    // event.
    aApp.downloadAvailable = true;
    aApp.downloadSize = manifest.size;
    aApp.updateManifest = aNewManifest;
    this._saveWidgetsFullPath(manifest, aApp);

    yield this._saveApps();

    this.broadcastMessage("Webapps:UpdateState", {
      app: aApp,
      id: aApp.id
    });
    this.broadcastMessage("Webapps:FireEvent", {
      eventType: "downloadavailable",
      manifestURL: aApp.manifestURL,
      requestID: aData.requestID
    });
  }),

  // A hosted app is updated if the app manifest or the appcache needs
  // updating. Even if the app manifest has not changed, we still check
  // for changes in the app cache.
  // 'aNewManifest' would contain the updated app manifest if
  // it has actually been updated, while 'aOldManifest' contains the
  // stored app manifest.
  updateHostedApp: Task.async(function*(aData, aId, aApp, aOldManifest, aNewManifest) {
    debug("updateHostedApp " + aData.manifestURL);

    // Clean up the deprecated manifest cache if needed.
    if (aId in this._manifestCache) {
      delete this._manifestCache[aId];
    }

    aApp.manifest = aNewManifest || aOldManifest;

    let manifest =
      new ManifestHelper(aApp.manifest, aApp.origin, aApp.manifestURL);
    aApp.role = manifest.role || "";

    if (!AppsUtils.checkAppRole(aApp.role, aApp.appStatus)) {
      this.broadcastMessage("Webapps:UpdateState", {
        app: aApp,
        manifest: aApp.manifest,
        id: aApp.id
      });
      this.broadcastMessage("Webapps:FireEvent", {
        eventType: "downloadapplied",
        manifestURL: aApp.manifestURL,
        requestID: aData.requestID
      });
      delete aApp.manifest;
      return;
    }

    if (aNewManifest) {
      this.updateAppHandlers(aOldManifest, aNewManifest, aApp);
      this.notifyUpdateHandlers(AppsUtils.cloneAppObject(aApp), aNewManifest);

      // Store the new manifest.
      let dir = this._getAppDir(aId).path;
      let manFile = OS.Path.join(dir, "manifest.webapp");
      yield this._writeFile(manFile, JSON.stringify(aNewManifest));

      manifest =
        new ManifestHelper(aNewManifest, aApp.origin, aApp.manifestURL);

      if (supportUseCurrentProfile()) {
        // Update the permissions for this app.
        PermissionsInstaller.installPermissions({
          manifest: aApp.manifest,
          origin: aApp.origin,
          manifestURL: aData.manifestURL,
          kind: aApp.kind
        }, true);
      }

      this.updateDataStore(this.webapps[aId].localId, aApp.origin,
                           aApp.manifestURL, aApp.manifest);

      aApp.name = aNewManifest.name;
      aApp.csp = manifest.csp || "";
      aApp.updateTime = Date.now();
    }

    // Update the registry.
    this.webapps[aId] = aApp;
    yield this._saveApps();

    if ((aApp.kind !== this.kHostedAppcache &&
         aApp.kind !== this.kTrustedHosted) ||
         !aApp.manifest.appcache_path) {
      this.broadcastMessage("Webapps:UpdateState", {
        app: aApp,
        manifest: aApp.manifest,
        id: aApp.id
      });
      this.broadcastMessage("Webapps:FireEvent", {
        eventType: "downloadapplied",
        manifestURL: aApp.manifestURL,
        requestID: aData.requestID
      });
    } else {
      // Check if the appcache is updatable, and send "downloadavailable" or
      // "downloadapplied".
      debug("updateHostedApp: updateSvc.checkForUpdate for " +
            manifest.fullAppcachePath());

      let updateDeferred = Promise.defer();

      updateSvc.checkForUpdate(Services.io.newURI(manifest.fullAppcachePath(), null, null),
                               aApp.localId, false,
                               (aSubject, aTopic, aData) => updateDeferred.resolve(aTopic));

      let topic = yield updateDeferred.promise;

      debug("updateHostedApp: updateSvc.checkForUpdate return for " +
            aApp.manifestURL + " - event is " + topic);

      let eventType =
        topic == "offline-cache-update-available" ? "downloadavailable"
                                                  : "downloadapplied";

      aApp.downloadAvailable = (eventType == "downloadavailable");
      yield this._saveApps();

      this.broadcastMessage("Webapps:UpdateState", {
        app: aApp,
        manifest: aApp.manifest,
        id: aApp.id
      });
      this.broadcastMessage("Webapps:FireEvent", {
        eventType: eventType,
        manifestURL: aApp.manifestURL,
        requestID: aData.requestID
      });
    }

    delete aApp.manifest;
  }),

  // Downloads the manifest and run checks, then eventually triggers the
  // installation UI.
  doInstall: function doInstall(aData, aMm) {
    let app = aData.app;

    let sendError = (aError) => {
      aData.error = aError;
      aMm.sendAsyncMessage("Webapps:Install:Return:KO", this.formatMessage(aData));
      Cu.reportError("Error installing app from: " + app.installOrigin +
                     ": " + aError);
      this.popContentAction(aData.oid);
    };

    if (app.receipts.length > 0) {
      for (let receipt of app.receipts) {
        let error = this.isReceipt(receipt);
        if (error) {
          sendError(error);
          return;
        }
      }
    }

    // Hosted apps can't be trusted or certified, so just check that the
    // manifest doesn't ask for those.
    function checkAppStatus(aManifest) {
      try {
        // Everything is authorized in developer mode.
        if (Services.prefs.getBoolPref("dom.apps.developer_mode")) {
          return true;
        }
      } catch(e) {}

      let manifestStatus = aManifest.type || "web";
      return manifestStatus === "web" ||
             manifestStatus === "trusted";
    }

    let checkManifest = (function() {
      if (!app.manifest) {
        sendError("MANIFEST_PARSE_ERROR");
        return false;
      }

      // Disallow reinstalls from the same manifest url for now.
      for (let id in this.webapps) {
        if (this.webapps[id].manifestURL == app.manifestURL &&
            this._isLaunchable(this.webapps[id])) {
          sendError("REINSTALL_FORBIDDEN");
          return false;
        }
      }

      if (!AppsUtils.checkManifest(app.manifest, app)) {
        sendError("INVALID_MANIFEST");
        return false;
      }

      if (!AppsUtils.checkInstallAllowed(app.manifest, app.installOrigin)) {
        sendError("INSTALL_FROM_DENIED");
        return false;
      }

      if (!checkAppStatus(app.manifest)) {
        sendError("INVALID_SECURITY_LEVEL");
        return false;
      }

      app.role = app.manifest.role || "";
      if (!AppsUtils.checkAppRole(app.role, app.appStatus)) {
        sendError("INVALID_ROLE");
        return false;
      }

      return true;
    }).bind(this);

    let installApp = (function() {
      app.manifestHash = this.computeManifestHash(app.manifest);

      // Check to see if the action has been cancelled in the interim.
      let cancelled = this.actionCancelled(aData.oid);
      this.popContentAction(aData.oid);
      if (!cancelled) {
        // We allow bypassing the install confirmation process to facilitate
        // automation.
        let prefName = "dom.mozApps.auto_confirm_install";
        if (Services.prefs.prefHasUserValue(prefName) &&
            Services.prefs.getBoolPref(prefName)) {
          this.confirmInstall(aData);
        } else {
          Services.obs.notifyObservers(aMm, "webapps-ask-install",
                                       JSON.stringify(aData));
        }
      }
    }).bind(this);

    // This action will be popped on success in installApp, or on
    // failure in sendError.
    this.pushContentAction(aData.oid);

    // We may already have the manifest (e.g. AutoInstall),
    // in which case we don't need to load it.
    if (app.manifest) {
      if (checkManifest()) {
        debug("Installed manifest check OK");
        if (this.kTrustedHosted !== this.appKind(app, app.manifest)) {
          installApp();
          return;
        }
        TrustedHostedAppsUtils.verifyManifest(aData.app, aData.appId, app.manifest)
          .then(installApp, sendError);
      } else {
        debug("Installed manifest check failed");
        // checkManifest() sends error before return
      }
      return;
    }

    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", app.manifestURL, true);
    xhr.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    xhr.channel.notificationCallbacks = AppsUtils.createLoadContext(aData.appId,
                                                                    aData.isBrowser);
    xhr.responseType = "json";

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200) {
        if (!AppsUtils.checkManifestContentType(app.installOrigin, app.origin,
                                                xhr.getResponseHeader("content-type"))) {
          sendError("INVALID_MANIFEST_CONTENT_TYPE");
          return;
        }

        app.manifest = xhr.response;
        if (checkManifest()) {
          debug("Downloaded manifest check OK");
          app.etag = xhr.getResponseHeader("Etag");
          if (this.kTrustedHosted !== this.appKind(app, app.manifest)) {
            installApp();
            return;
          }

          debug("App kind: " + this.kTrustedHosted);
          TrustedHostedAppsUtils.verifyManifest(aData.app, aData.appId, app.manifest)
            .then(installApp, sendError);
          return;
        } else {
          debug("Downloaded manifest check failed");
          // checkManifest() sends error before return
        }
      } else {
        sendError("MANIFEST_URL_ERROR");
      }
    }).bind(this), false);

    xhr.addEventListener("error", (function() {
      sendError("NETWORK_ERROR");
    }).bind(this), false);

    xhr.send(null);
  },

  doInstallPackage: function doInstallPackage(aData, aMm) {
    let app = aData.app;

    let sendError = (aError) => {
      aData.error = aError;
      aMm.sendAsyncMessage("Webapps:Install:Return:KO", this.formatMessage(aData));
      Cu.reportError("Error installing packaged app from: " +
                     app.installOrigin + ": " + aError);
      this.popContentAction(aData.oid);
    };

    if (app.receipts.length > 0) {
      for (let receipt of app.receipts) {
        let error = this.isReceipt(receipt);
        if (error) {
          sendError(error);
          return;
        }
      }
    }

    let checkUpdateManifest = (function() {
      let manifest = app.updateManifest;

      // Disallow reinstalls from the same manifest URL for now.
      let id = this._appIdForManifestURL(app.manifestURL);
      if (id !== null && this._isLaunchable(this.webapps[id])) {
        sendError("REINSTALL_FORBIDDEN");
        return false;
      }

      if (!(AppsUtils.checkManifest(manifest, app) && manifest.package_path)) {
        sendError("INVALID_MANIFEST");
        return false;
      }

      if (!AppsUtils.checkInstallAllowed(manifest, app.installOrigin)) {
        sendError("INSTALL_FROM_DENIED");
        return false;
      }

      return true;
    }).bind(this);

    let installApp = (function() {
      app.manifestHash = this.computeManifestHash(app.updateManifest);

      // Check to see if the action has been cancelled in the interim.
      let cancelled = this.actionCancelled(aData.oid);
      this.popContentAction(aData.oid);
      if (!cancelled) {
        // We allow bypassing the install confirmation process to facilitate
        // automation.
        let prefName = "dom.mozApps.auto_confirm_install";
        if (Services.prefs.prefHasUserValue(prefName) &&
            Services.prefs.getBoolPref(prefName)) {
          this.confirmInstall(aData);
        } else {
          Services.obs.notifyObservers(aMm, "webapps-ask-install",
                                       JSON.stringify(aData));
        }
      }
    }).bind(this);

    // This action will be popped on success in installApp, or on
    // failure in sendError.
    this.pushContentAction(aData.oid);

    // We may already have the manifest (e.g. AutoInstall),
    // in which case we don't need to load it.
    if (app.updateManifest) {
      if (checkUpdateManifest()) {
        installApp();
      }
      return;
    }

    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", app.manifestURL, true);
    xhr.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    xhr.channel.notificationCallbacks = AppsUtils.createLoadContext(aData.appId,
                                                                    aData.isBrowser);
    xhr.responseType = "json";

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200) {
        if (!AppsUtils.checkManifestContentType(app.installOrigin, app.origin,
                                                xhr.getResponseHeader("content-type"))) {
          sendError("INVALID_MANIFEST_CONTENT_TYPE");
          return;
        }

        app.updateManifest = xhr.response;
        if (!app.updateManifest) {
          sendError("MANIFEST_PARSE_ERROR");
          return;
        }
        if (checkUpdateManifest()) {
          app.etag = xhr.getResponseHeader("Etag");
          debug("at install package got app etag=" + app.etag);
          installApp();
        }
      }
      else {
        sendError("MANIFEST_URL_ERROR");
      }
    }).bind(this), false);

    xhr.addEventListener("error", (function() {
      sendError("NETWORK_ERROR");
    }).bind(this), false);

    xhr.send(null);
  },

  onLocationChange(oid) {
    let action = this._contentActions.get(oid);
    if (action) {
      action.cancelled = true;
    }
  },

  pushContentAction: function(windowID) {
    let actions = this._contentActions.get(windowID);
    if (!actions) {
      actions = {
        count: 0,
        cancelled: false,
      };
      this._contentActions.set(windowID, actions);
    }
    actions.count++;
  },

  popContentAction: function(windowID) {
    let actions = this._contentActions.get(windowID);
    if (!actions) {
      Cu.reportError(`Failed to pop content action for window with ID ${windowID}`);
      return;
    }
    actions.count--;
    if (!actions.count) {
      this._contentActions.delete(windowID);
    }
  },

  actionCancelled: function(windowID) {
    return this._contentActions.has(windowID) &&
           this._contentActions.get(windowID).cancelled;
  },

  denyInstall: function(aData) {
    let packageId = aData.app.packageId;
    if (packageId) {
      let dir = FileUtils.getDir("TmpD", ["webapps", packageId],
                                 true, true);
      try {
        dir.remove(true);
      } catch(e) {
      }
    }
    aData.mm.sendAsyncMessage("Webapps:Install:Return:KO", this.formatMessage(aData));
  },

  // This function is called after we called the onsuccess callback on the
  // content side. This let the webpage the opportunity to set event handlers
  // on the app before we start firing progress events.
  queuedDownload: {},
  queuedPackageDownload: {},

  onInstallSuccessAck: Task.async(function*(aManifestURL, aDontNeedNetwork) {
    // If we are offline, register to run when we'll be online.
    if ((Services.io.offline) && !aDontNeedNetwork) {
      let onlineWrapper = {
        observe: function(aSubject, aTopic, aData) {
          Services.obs.removeObserver(onlineWrapper,
                                      "network:offline-status-changed");
          DOMApplicationRegistry.onInstallSuccessAck(aManifestURL);
        }
      };
      Services.obs.addObserver(onlineWrapper,
                               "network:offline-status-changed", false);
      return;
    }

    let cacheDownload = this.queuedDownload[aManifestURL];
    if (cacheDownload) {
      this.startOfflineCacheDownload(cacheDownload.manifest,
                                     cacheDownload.app,
                                     cacheDownload.profileDir);
      delete this.queuedDownload[aManifestURL];

      return;
    }

    let packageDownload = this.queuedPackageDownload[aManifestURL];
    if (packageDownload) {
      let manifest = packageDownload.manifest;
      let newApp = packageDownload.app;
      let installSuccessCallback = packageDownload.callback;

      delete this.queuedPackageDownload[aManifestURL];

      let id = this._appIdForManifestURL(newApp.manifestURL);
      let oldApp = this.webapps[id];
      let newManifest, newId;
      try {
        [newId, newManifest] = yield this.downloadPackage(id, oldApp, manifest, newApp, false);

        yield this._onDownloadPackage(newApp, installSuccessCallback, newId, newManifest);
      } catch (ex) {
        this.revertDownloadPackage(id, oldApp, newApp, false, ex);
      }
    }
  }),

  _setupApp: function(aData, aId) {
    let app = aData.app;

    // app can be uninstalled
    app.removable = true;

    if (aData.isPackage) {
      // Override the origin with the correct id.
      app.origin = "app://" + aId;
    }

    app.id = aId;
    app.installTime = Date.now();
    app.lastUpdateCheck = Date.now();

    return app;
  },

  _cloneApp: function(aData, aNewApp, aLocaleManifest, aManifest, aId, aLocalId) {
    let appObject = AppsUtils.cloneAppObject(aNewApp);
    appObject.appStatus =
      aNewApp.appStatus || Ci.nsIPrincipal.APP_STATUS_INSTALLED;

    let usesAppcache = appObject.kind == this.kHostedAppcache;
    if (appObject.kind == this.kTrustedHosted && aManifest.appcache_path) {
      usesAppcache = true;
    }

    if (usesAppcache) {
      appObject.installState = "pending";
      appObject.downloadAvailable = true;
      appObject.downloading = true;
      appObject.downloadSize = 0;
      appObject.readyToApplyDownload = false;
    } else if (appObject.kind == this.kPackaged) {
      appObject.installState = "pending";
      appObject.downloadAvailable = true;
      appObject.downloading = true;
      appObject.downloadSize = aLocaleManifest.size;
      appObject.readyToApplyDownload = false;
    } else if (appObject.kind == this.kHosted ||
               appObject.kind == this.kTrustedHosted) {
      appObject.installState = "installed";
      appObject.downloadAvailable = false;
      appObject.downloading = false;
      appObject.readyToApplyDownload = false;
    } else {
      debug("Unknown app kind: " + appObject.kind);
      throw Error("Unknown app kind: " + appObject.kind);
    }

    appObject.localId = aLocalId;
    appObject.basePath = OS.Path.dirname(this.appsFile);
    appObject.name = aManifest.name;
    appObject.csp = aLocaleManifest.csp || "";
    appObject.role = aLocaleManifest.role;
    this._saveWidgetsFullPath(aLocaleManifest, appObject);
    appObject.installerAppId = aData.appId;
    appObject.installerIsBrowser = aData.isBrowser;

    return appObject;
  },

  _writeManifestFile: function(aId, aIsPackage, aJsonManifest) {
    debug("_writeManifestFile");

    // For packaged apps, keep the update manifest distinct from the app manifest.
    let manifestName = aIsPackage ? "update.webapp" : "manifest.webapp";

    let dir = this._getAppDir(aId).path;
    let manFile = OS.Path.join(dir, manifestName);
    return this._writeFile(manFile, JSON.stringify(aJsonManifest));
  },

  // Add an app that is already installed to the registry.
  addInstalledApp: Task.async(function*(aApp, aManifest, aUpdateManifest) {
    if (this.getAppLocalIdByManifestURL(aApp.manifestURL) !=
        Ci.nsIScriptSecurityManager.NO_APP_ID) {
      return;
    }

    let app = AppsUtils.cloneAppObject(aApp);

    if (!AppsUtils.checkManifest(aManifest, app) ||
        (aUpdateManifest && !AppsUtils.checkManifest(aUpdateManifest, app))) {
      return;
    }

    app.name = aManifest.name;

    app.csp = aManifest.csp || "";

    let aLocaleManifest = new ManifestHelper(aManifest, app.origin, app.manifestURL);
    this._saveWidgetsFullPath(aLocaleManifest, app);

    app.appStatus = AppsUtils.getAppManifestStatus(aManifest);

    app.removable = true;

    // Reuse the app ID if the scheme is "app".
    let uri = Services.io.newURI(app.origin, null, null);
    if (uri.scheme == "app") {
      app.id = uri.host;
    } else {
      app.id = this.makeAppId();
    }

    app.localId = this._nextLocalId();

    app.basePath = OS.Path.dirname(this.appsFile);

    app.progress = 0.0;
    app.installState = "installed";
    app.downloadAvailable = false;
    app.downloading = false;
    app.readyToApplyDownload = false;

    if (aUpdateManifest && aUpdateManifest.size) {
      app.downloadSize = aUpdateManifest.size;
    }

    app.manifestHash = AppsUtils.computeHash(JSON.stringify(aUpdateManifest ||
                                                            aManifest));

    let zipFile = WebappOSUtils.getPackagePath(app);
    app.packageHash = yield this._computeFileHash(zipFile);

    app.role = aManifest.role || "";
    if (!AppsUtils.checkAppRole(app.role, app.appStatus)) {
      return;
    }

    app.redirects = this.sanitizeRedirects(aManifest.redirects);

    this.webapps[app.id] = app;

    // Store the manifest in the manifest cache, so we don't need to re-read it
    this._manifestCache[app.id] = app.manifest;

    // Store the manifest and the updateManifest.
    this._writeManifestFile(app.id, false, aManifest);
    if (aUpdateManifest) {
      this._writeManifestFile(app.id, true, aUpdateManifest);
    }

    this._saveApps().then(() => {
      this.broadcastMessage("Webapps:AddApp",
                            { id: app.id, app: app, manifest: aManifest });
    });
  }),

  confirmInstall: Task.async(function*(aData, aProfileDir, aInstallSuccessCallback) {
    debug("confirmInstall");

    let origin = Services.io.newURI(aData.app.origin, null, null);
    let id = this._appIdForManifestURL(aData.app.manifestURL);
    let manifestURL = origin.resolve(aData.app.manifestURL);
    let localId = this.getAppLocalIdByManifestURL(manifestURL);

    let isReinstall = false;

    // Installing an application again is considered as an update.
    if (id) {
      isReinstall = true;
      let dir = this._getAppDir(id);
      try {
        dir.remove(true);
      } catch(e) { }
    } else {
      id = this.makeAppId();
      localId = this._nextLocalId();
    }

    let app = this._setupApp(aData, id);

    let jsonManifest = aData.isPackage ? app.updateManifest : app.manifest;
    yield this._writeManifestFile(id, aData.isPackage, jsonManifest);

    debug("app.origin: " + app.origin);
    let manifest =
      new ManifestHelper(jsonManifest, app.origin, app.manifestURL);

    // Set the application kind.
    app.kind = this.appKind(app, manifest);

    let appObject = this._cloneApp(aData, app, manifest, jsonManifest, id, localId);

    this.webapps[id] = appObject;

    // For package apps, the permissions are not in the mini-manifest, so
    // don't update the permissions yet.
    if (!aData.isPackage) {
      if (supportUseCurrentProfile()) {
        try {
          if (Services.prefs.getBoolPref("dom.apps.developer_mode")) {
            this.webapps[id].appStatus =
              AppsUtils.getAppManifestStatus(app.manifest);
          }
        } catch(e) {};
        PermissionsInstaller.installPermissions(
          {
            origin: appObject.origin,
            manifestURL: appObject.manifestURL,
            manifest: jsonManifest,
            kind: appObject.kind
          },
          isReinstall,
          this.doUninstall.bind(this, aData, aData.mm)
        );
      }

      this.updateDataStore(this.webapps[id].localId,  this.webapps[id].origin,
                           this.webapps[id].manifestURL, jsonManifest);
    }

    for (let prop of ["installState", "downloadAvailable", "downloading",
                           "downloadSize", "readyToApplyDownload"]) {
      aData.app[prop] = appObject[prop];
    }

    let dontNeedNetwork = false;

    if ((appObject.kind == this.kHostedAppcache ||
        appObject.kind == this.kTrustedHosted) &&
        manifest.appcache_path) {
      this.queuedDownload[app.manifestURL] = {
        manifest: manifest,
        app: appObject,
        profileDir: aProfileDir
      }
    } else if (appObject.kind == this.kPackaged) {
      // If it is a local app then it must been installed from a local file
      // instead of web.
      // In that case, we would already have the manifest, not just the update
      // manifest.
#ifdef MOZ_WIDGET_ANDROID
      dontNeedNetwork = !!aData.app.manifest;
#else
      if (aData.app.localInstallPath) {
        dontNeedNetwork = true;
        jsonManifest.package_path = "file://" + aData.app.localInstallPath;
      }
#endif

      // origin for install apps is meaningless here, since it's app:// and this
      // can't be used to resolve package paths.
      manifest = new ManifestHelper(jsonManifest, app.origin, app.manifestURL);

      this.queuedPackageDownload[app.manifestURL] = {
        manifest: manifest,
        app: appObject,
        callback: aInstallSuccessCallback
      };
    }

    // We notify about the successful installation via mgmt.oninstall and the
    // corresponding DOMRequest.onsuccess event as soon as the app is properly
    // saved in the registry.
    yield this._saveApps();

    aData.isPackage ? appObject.updateManifest = jsonManifest :
                      appObject.manifest = jsonManifest;
    this.broadcastMessage("Webapps:AddApp", { id: id, app: appObject });

    if (!aData.isPackage) {
      this.updateAppHandlers(null, app.manifest, app);
      if (aInstallSuccessCallback) {
        try {
          yield aInstallSuccessCallback(app, app.manifest);
        } catch (e) {
          // Ignore exceptions during the local installation of
          // an app. If it fails, the app will anyway be considered
          // as not installed because isLaunchable will return false.
        }
      }
    }

    // The presence of a requestID means that we have a page to update.
    if (aData.isPackage && aData.apkInstall && !aData.requestID) {
      // Skip directly to onInstallSuccessAck, since there isn't
      // a WebappsRegistry to receive Webapps:Install:Return:OK and respond
      // Webapps:Install:Return:Ack when an app is being auto-installed.
      this.onInstallSuccessAck(app.manifestURL);
    } else {
      // Broadcast Webapps:Install:Return:OK so the WebappsRegistry can notify
      // the installing page about the successful install, after which it'll
      // respond Webapps:Install:Return:Ack, which calls onInstallSuccessAck.
      this.broadcastMessage("Webapps:Install:Return:OK", aData);
    }

    Services.obs.notifyObservers(null, "webapps-installed",
      JSON.stringify({ manifestURL: app.manifestURL }));

    if (aData.forceSuccessAck) {
      // If it's a local install, there's no content process so just
      // ack the install.
      this.onInstallSuccessAck(app.manifestURL, dontNeedNetwork);
    }
  }),

/**
   * Install the package after successfully downloading it
   *
   * Bound params:
   *
   * @param aNewApp {Object} the new app data
   * @param aInstallSuccessCallback {Function}
   *        the callback to call on install success
   *
   * Passed params:
   *
   * @param aId {Integer} the unique ID of the application
   * @param aManifest {Object} The manifest of the application
   */
  _onDownloadPackage: Task.async(function*(aNewApp, aInstallSuccessCallback,
                                           aId, aManifest) {
    debug("_onDownloadPackage");
    // Success! Move the zip out of TmpD.
    let app = this.webapps[aId];
    let zipFile =
      FileUtils.getFile("TmpD", ["webapps", aId, "application.zip"], true);
    let dir = this._getAppDir(aId);
    zipFile.moveTo(dir, "application.zip");
    let tmpDir = FileUtils.getDir("TmpD", ["webapps", aId], true, true);
    try {
      tmpDir.remove(true);
    } catch(e) { }

    // Save the manifest
    let manFile = OS.Path.join(dir.path, "manifest.webapp");
    yield this._writeFile(manFile, JSON.stringify(aManifest));
    // Set state and fire events.
    app.installState = "installed";
    app.downloading = false;
    app.downloadAvailable = false;

    yield this._saveApps();

    this.updateAppHandlers(null, aManifest, aNewApp);
    // Clear the manifest cache in case it holds the update manifest.
    if (aId in this._manifestCache) {
      delete this._manifestCache[aId];
    }

    this.broadcastMessage("Webapps:AddApp",
                          { id: aId, app: aNewApp, manifest: aManifest });
    Services.obs.notifyObservers(null, "webapps-installed",
      JSON.stringify({ manifestURL: aNewApp.manifestURL }));

    if (supportUseCurrentProfile()) {
      // Update the permissions for this app.
      PermissionsInstaller.installPermissions({
        manifest: aManifest,
        origin: aNewApp.origin,
        manifestURL: aNewApp.manifestURL,
        kind: this.webapps[aId].kind
      }, true);
    }

    this.updateDataStore(this.webapps[aId].localId, aNewApp.origin,
                         aNewApp.manifestURL, aManifest);

    if (aInstallSuccessCallback) {
      try {
        yield aInstallSuccessCallback(aNewApp, aManifest, zipFile.path);
      } catch (e) {
        // Ignore exceptions during the local installation of
        // an app. If it fails, the app will anyway be considered
        // as not installed because isLaunchable will return false.
      }
    }

    this.broadcastMessage("Webapps:UpdateState", {
      app: app,
      manifest: aManifest,
      manifestURL: aNewApp.manifestURL
    });

    // Check if we have asm.js code to preload for this application.
    yield ScriptPreloader.preload(aNewApp, aManifest);

    // Update langpack information.
    yield Langpacks.register(aNewApp, aManifest);

    this.broadcastMessage("Webapps:FireEvent", {
      eventType: ["downloadsuccess", "downloadapplied"],
      manifestURL: aNewApp.manifestURL
    });
  }),

  _nextLocalId: function() {
    let id = Services.prefs.getIntPref("dom.mozApps.maxLocalId") + 1;

    while (this.getManifestURLByLocalId(id)) {
      id++;
    }

    Services.prefs.setIntPref("dom.mozApps.maxLocalId", id);
    Services.prefs.savePrefFile(null);
    return id;
  },

  _appIdForManifestURL: function(aURI) {
    for (let id in this.webapps) {
      if (this.webapps[id].manifestURL == aURI)
        return id;
    }
    return null;
  },

  makeAppId: function() {
    let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
    return uuidGenerator.generateUUID().toString();
  },

  _saveApps: function() {
    return this._writeFile(this.appsFile, JSON.stringify(this.webapps, null, 2));
  },

  /**
    * Asynchronously reads a list of manifests
    */

  _manifestCache: {},

  _readManifests: function(aData) {
    let manifestCache = this._manifestCache;
    return Task.spawn(function*() {
      for (let elem of aData) {
        let id = elem.id;

        if (!manifestCache[id]) {
          // the manifest file used to be named manifest.json, so fallback on this.
          let baseDir = this.webapps[id].basePath == this.getCoreAppsBasePath()
                          ? "coreAppsDir" : DIRECTORY_NAME;

          let dir = FileUtils.getDir(baseDir, ["webapps", id], false, true);

          let fileNames = ["manifest.webapp", "update.webapp", "manifest.json"];
          for (let fileName of fileNames) {
            manifestCache[id] = yield AppsUtils.loadJSONAsync(OS.Path.join(dir.path, fileName));
            if (manifestCache[id]) {
              break;
            }
          }
        }

        elem.manifest = manifestCache[id];
      }

      return aData;
    }.bind(this)).then(null, Cu.reportError);
  },

  downloadPackage: Task.async(function*(aId, aOldApp, aManifest, aNewApp, aIsUpdate) {
    // Here are the steps when installing a package:
    // - create a temp directory where to store the app.
    // - download the zip in this directory.
    // - check the signature on the zip.
    // - extract the manifest from the zip and check it.
    // - ask confirmation to the user.
    // - add the new app to the registry.
    yield this._ensureSufficientStorage(aNewApp);

    let fullPackagePath = aManifest.fullPackagePath();
    // Check if it's a local file install (we've downloaded/sideloaded the
    // package already, it existed on the build, or it came with an APK).
    // Note that this variable also controls whether files signed with expired
    // certificates are accepted or not. If isLocalFileInstall is true and the
    // device date is earlier than the build generation date, then the signature
    // will be accepted even if the certificate is expired.
    let isLocalFileInstall =
      Services.io.extractScheme(fullPackagePath) === 'file';

    debug("About to download " + fullPackagePath);

    let requestChannel = this._getRequestChannel(fullPackagePath,
                                                 isLocalFileInstall,
                                                 aOldApp,
                                                 aNewApp);

    AppDownloadManager.add(
      aNewApp.manifestURL,
      {
        channel: requestChannel,
        appId: aId,
        previousState: aIsUpdate ? "installed" : "pending"
      }
    );

    // We set the 'downloading' flag to true right before starting the fetch.
    aOldApp.downloading = true;

    // We determine the app's 'installState' according to its previous
    // state. Cancelled download should remain as 'pending'. Successfully
    // installed apps should morph to 'updating'.
    aOldApp.installState = aIsUpdate ? "updating" : "pending";

    // initialize the progress to 0 right now
    aOldApp.progress = 0;

    // Save the current state of the app to handle cases where we may be
    // retrying a past download.
    yield DOMApplicationRegistry._saveApps();

    DOMApplicationRegistry.broadcastMessage("Webapps:UpdateState", {
        // Clear any previous download errors.
        error: null,
        app: aOldApp,
        id: aId
    });

    let zipFile = yield this._getPackage(requestChannel, aId, aOldApp, aNewApp);

    // After this point, it's too late to cancel the download.
    AppDownloadManager.remove(aNewApp.manifestURL);

    let responseStatus = requestChannel.responseStatus;
    let oldPackage = responseStatus == 304;

    // If the response was 304 we probably won't have anything to hash.
    let hash = null;
    if (!oldPackage) {
      hash = yield this._computeFileHash(zipFile.path);
    }

    oldPackage = oldPackage || (hash == aOldApp.packageHash);

    if (oldPackage) {
      debug("package's etag or hash unchanged; sending 'applied' event");
      // The package's Etag or hash has not changed.
      // We send an "applied" event right away so code awaiting that event
      // can proceed to access the app. We also throw an error to alert
      // the caller that the package wasn't downloaded.
      this._sendAppliedEvent(aOldApp);
      throw "PACKAGE_UNCHANGED";
    }

    let newManifest = yield this._openAndReadPackage(zipFile, aOldApp, aNewApp,
            isLocalFileInstall, aIsUpdate, aManifest, requestChannel, hash);

    return [aOldApp.id, newManifest];

  }),

  _ensureSufficientStorage: function(aNewApp) {
    let deferred = Promise.defer();

    let navigator = Services.wm.getMostRecentWindow(chromeWindowType)
                            .navigator;
    let deviceStorage = null;

    if (navigator.getDeviceStorage) {
      deviceStorage = navigator.getDeviceStorage("apps");
    }

    if (deviceStorage) {
      let req = deviceStorage.freeSpace();
      req.onsuccess = req.onerror = e => {
        let freeBytes = e.target.result;
        let sufficientStorage = this._checkDownloadSize(freeBytes, aNewApp);
        if (sufficientStorage) {
          deferred.resolve();
        } else {
          deferred.reject("INSUFFICIENT_STORAGE");
        }
      }
    } else {
      debug("No deviceStorage");
      // deviceStorage isn't available, so use FileUtils to find the size of
      // available storage.
      let dir = FileUtils.getDir(DIRECTORY_NAME, ["webapps"], true, true);
      try {
        let sufficientStorage = this._checkDownloadSize(dir.diskSpaceAvailable,
                                                        aNewApp);
        if (sufficientStorage) {
          deferred.resolve();
        } else {
          deferred.reject("INSUFFICIENT_STORAGE");
        }
      } catch(ex) {
        // If disk space information isn't available, we'll end up here.
        // We should proceed anyway, otherwise devices that support neither
        // deviceStorage nor diskSpaceAvailable will never be able to install
        // packaged apps.
        deferred.resolve();
      }
    }

    return deferred.promise;
  },

  _checkDownloadSize: function(aFreeBytes, aNewApp) {
    if (aFreeBytes) {
      debug("Free storage: " + aFreeBytes + ". Download size: " +
            aNewApp.downloadSize);
      if (aFreeBytes <=
          aNewApp.downloadSize + AppDownloadManager.MIN_REMAINING_FREESPACE) {
        return false;
      }
    }
    return true;
  },

  _getRequestChannel: function(aFullPackagePath, aIsLocalFileInstall, aOldApp,
                               aNewApp) {
    let requestChannel;

    let appURI = NetUtil.newURI(aNewApp.origin, null, null);
    let principal = Services.scriptSecurityManager.getAppCodebasePrincipal(
                      appURI, aNewApp.localId, false);

    if (aIsLocalFileInstall) {
      requestChannel = NetUtil.newChannel({
        uri: aFullPackagePath,
        loadingPrincipal: principal,
        contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER}
      ).QueryInterface(Ci.nsIFileChannel);
    } else {
      requestChannel = NetUtil.newChannel({
        uri: aFullPackagePath,
        loadingPrincipal: principal,
        contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER}
      ).QueryInterface(Ci.nsIHttpChannel);
      requestChannel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    }

    if (aOldApp.packageEtag && !aIsLocalFileInstall) {
      debug("Add If-None-Match header: " + aOldApp.packageEtag);
      requestChannel.setRequestHeader("If-None-Match", aOldApp.packageEtag,
                                      false);
    }

    let lastProgressTime = 0;

    requestChannel.notificationCallbacks = {
      QueryInterface: function(aIID) {
        if (aIID.equals(Ci.nsISupports)          ||
            aIID.equals(Ci.nsIProgressEventSink) ||
            aIID.equals(Ci.nsILoadContext))
          return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
      },
      getInterface: function(aIID) {
        return this.QueryInterface(aIID);
      },
      onProgress: (function(aRequest, aContext, aProgress, aProgressMax) {
        aOldApp.progress = aProgress;
        let now = Date.now();
        if (now - lastProgressTime > MIN_PROGRESS_EVENT_DELAY) {
          debug("onProgress: " + aProgress + "/" + aProgressMax);
          this._sendDownloadProgressEvent(aNewApp, aProgress);
          lastProgressTime = now;
          this._saveApps();
        }
      }).bind(this),
      onStatus: function(aRequest, aContext, aStatus, aStatusArg) { },

      // nsILoadContext
      appId: aOldApp.installerAppId,
      isInBrowserElement: aOldApp.installerIsBrowser,
      usePrivateBrowsing: false,
      isContent: false,
      associatedWindow: null,
      topWindow : null,
      isAppOfType: function(appType) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      }
    };

    return requestChannel;
  },

  _sendDownloadProgressEvent: function(aNewApp, aProgress) {
    this.broadcastMessage("Webapps:UpdateState", {
      app: {
        progress: aProgress
      },
      id: aNewApp.id
    });
    this.broadcastMessage("Webapps:FireEvent", {
      eventType: "progress",
      manifestURL: aNewApp.manifestURL
    });
  },

  _getPackage: function(aRequestChannel, aId, aOldApp, aNewApp) {
    let deferred = Promise.defer();

    AppsUtils.getFile(aRequestChannel, aId, "application.zip").then((aFile) => {
      deferred.resolve(aFile);
    }, function(rejectStatus) {
      debug("Failed to download package file: " + rejectStatus.msg);
      if (!rejectStatus.downloadAvailable) {
        aOldApp.downloadAvailable = false;
      }
      deferred.reject(rejectStatus.msg);
    });

    // send a first progress event to correctly set the DOM object's properties
    this._sendDownloadProgressEvent(aNewApp, 0);

    return deferred.promise;
  },

  /**
   * Compute the MD5 hash of a file, doing async IO off the main thread.
   *
   * @param   {String} aFilePath
   *                   the path of the file to hash
   * @returns {String} the MD5 hash of the file
   */
  _computeFileHash: function(aFilePath) {
    let deferred = Promise.defer();

    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(aFilePath);

    NetUtil.asyncFetch({
      uri: NetUtil.newURI(file),
      loadUsingSystemPrincipal: true
    }, function(inputStream, status) {
      if (!Components.isSuccessCode(status)) {
        debug("Error reading " + aFilePath + ": " + e);
        deferred.reject();
        return;
      }

      let hasher = Cc["@mozilla.org/security/hash;1"]
                     .createInstance(Ci.nsICryptoHash);
      // We want to use the MD5 algorithm.
      hasher.init(hasher.MD5);

      const PR_UINT32_MAX = 0xffffffff;
      hasher.updateFromStream(inputStream, PR_UINT32_MAX);

      // Return the two-digit hexadecimal code for a byte.
      function toHexString(charCode) {
        return ("0" + charCode.toString(16)).slice(-2);
      }

      // We're passing false to get the binary hash and not base64.
      let data = hasher.finish(false);
      // Convert the binary hash data to a hex string.
      let hash = [toHexString(data.charCodeAt(i)) for (i in data)].join("");
      debug("File hash computed: " + hash);

      deferred.resolve(hash);
    });

    return deferred.promise;
  },

  /**
   * Send an "applied" event right away for the package being installed.
   *
   * XXX We use this to exit the app update process early when the downloaded
   * package is identical to the last one we installed.  Presumably we do
   * something similar after updating the app, and we could refactor both cases
   * to use the same code to send the "applied" event.
   *
   * @param aApp {Object} app data
   */
  _sendAppliedEvent: function(aApp) {
    aApp.downloading = false;
    aApp.downloadAvailable = false;
    aApp.downloadSize = 0;
    aApp.installState = "installed";
    aApp.readyToApplyDownload = false;
    if (aApp.staged && aApp.staged.manifestHash) {
      // If we're here then the manifest has changed but the package
      // hasn't. Let's clear this, so we don't keep offering
      // a bogus update to the user
      aApp.manifestHash = aApp.staged.manifestHash;
      aApp.etag = aApp.staged.etag || aApp.etag;
      aApp.staged = {};
     // Move the staged update manifest to a non staged one.
      try {
        let staged = this._getAppDir(aApp.id);
        staged.append("staged-update.webapp");
        staged.moveTo(staged.parent, "update.webapp");
      } catch (ex) {
        // We don't really mind much if this fails.
      }
    }

    // Save the updated registry, and cleanup the tmp directory.
    this._saveApps().then(() => {
      this.broadcastMessage("Webapps:UpdateState", {
        app: aApp,
        id: aApp.id
      });
      this.broadcastMessage("Webapps:FireEvent", {
        manifestURL: aApp.manifestURL,
        eventType: ["downloadsuccess", "downloadapplied"]
      });
    });
    let file = FileUtils.getFile("TmpD", ["webapps", aApp.id], false);
    if (file && file.exists()) {
      file.remove(true);
    }
  },

  _openAndReadPackage: function(aZipFile, aOldApp, aNewApp, aIsLocalFileInstall,
                                aIsUpdate, aManifest, aRequestChannel, aHash) {
    return Task.spawn((function*() {
      let zipReader, isSigned, newManifest;

      try {
        [zipReader, isSigned] = yield this._openPackage(aZipFile, aOldApp,
                                                        aIsLocalFileInstall);
        newManifest = yield this._readPackage(aOldApp, aNewApp,
                aIsLocalFileInstall, aIsUpdate, aManifest, aRequestChannel,
                aHash, zipReader, isSigned);
      } catch (e) {
        debug("package open/read error: " + e);
        // Something bad happened when opening/reading the package.
        // Unrecoverable error, don't bug the user.
        // Apps with installState 'pending' does not produce any
        // notification, so we are safe with its current
        // downloadAvailable state.
        if (aOldApp.installState !== "pending") {
          aOldApp.downloadAvailable = false;
        }
        if (typeof e == 'object') {
          Cu.reportError("Error while reading package: " + e + "\n" + e.stack);
          throw "INVALID_PACKAGE";
        } else {
          throw e;
        }
      } finally {
        if (zipReader) {
          zipReader.close();
        }
      }

      return newManifest;

    }).bind(this));
  },

  _openPackage: function(aZipFile, aApp, aIsLocalFileInstall) {
    return Task.spawn((function*() {
      let certDb;
      try {
        certDb = Cc["@mozilla.org/security/x509certdb;1"]
                   .getService(Ci.nsIX509CertDB);
      } catch (e) {
        debug("nsIX509CertDB error: " + e);
        // unrecoverable error, don't bug the user
        aApp.downloadAvailable = false;
        throw "CERTDB_ERROR";
      }

      let [result, zipReader] = yield this._openSignedPackage(aApp.installOrigin,
                                                              aApp.manifestURL,
                                                              aZipFile,
                                                              certDb);

      // We cannot really know if the system date is correct or
      // not. What we can know is if it's after the build date or not,
      // and assume the build date is correct (which we cannot
      // really know either).
      let isLaterThanBuildTime = Date.now() > PLATFORM_BUILD_ID_TIME;

      let isSigned;

      if (Components.isSuccessCode(result)) {
        isSigned = true;
      } else if (result == Cr.NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY ||
                 result == Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY ||
                 result == Cr.NS_ERROR_SIGNED_JAR_ENTRY_MISSING) {
        throw "APP_PACKAGE_CORRUPTED";
      } else if (result == Cr.NS_ERROR_FILE_CORRUPTED ||
                 result == Cr.NS_ERROR_SIGNED_JAR_ENTRY_TOO_LARGE ||
                 result == Cr.NS_ERROR_SIGNED_JAR_ENTRY_INVALID ||
                 result == Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID) {
        throw "APP_PACKAGE_INVALID";
      } else if ((!aIsLocalFileInstall || isLaterThanBuildTime) &&
                 (result != Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED)) {
        throw "INVALID_SIGNATURE";
      } else {
        // If it's a localFileInstall and the validation failed
        // because of a expired certificate, just assume it was valid
        // and that the error occurred because the system time has not
        // been set yet.
        isSigned = (aIsLocalFileInstall &&
                    (getNSPRErrorCode(result) ==
                     SEC_ERROR_EXPIRED_CERTIFICATE));

        zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                      .createInstance(Ci.nsIZipReader);
        zipReader.open(aZipFile);
      }

      return [zipReader, isSigned];

    }).bind(this));
  },

  _openSignedPackage: function(aInstallOrigin, aManifestURL, aZipFile, aCertDb) {
    let deferred = Promise.defer();

    let root = TrustedRootCertificate.index;

    let useReviewerCerts = false;
    try {
      useReviewerCerts = Services.prefs.
                           getBoolPref("dom.mozApps.use_reviewer_certs");
    } catch (ex) { }

    // We'll use the reviewer and dev certificates only if the pref is set to
    // true.
    if (useReviewerCerts) {
      let manifestPath = Services.io.newURI(aManifestURL, null, null).path;

      switch (aInstallOrigin) {
        case "https://marketplace.firefox.com":
          root = manifestPath.startsWith("/reviewers/")
               ? Ci.nsIX509CertDB.AppMarketplaceProdReviewersRoot
               : Ci.nsIX509CertDB.AppMarketplaceProdPublicRoot;
          break;

        case "https://marketplace-dev.allizom.org":
          root = manifestPath.startsWith("/reviewers/")
               ? Ci.nsIX509CertDB.AppMarketplaceDevReviewersRoot
               : Ci.nsIX509CertDB.AppMarketplaceDevPublicRoot;
          break;

        // The staging server uses the same certificate for both
        // public and unreviewed apps.
        case "https://marketplace.allizom.org":
          root = Ci.nsIX509CertDB.AppMarketplaceStageRoot;
          break;
      }
    }

    aCertDb.openSignedAppFileAsync(
       root, aZipFile,
       function(aRv, aZipReader) {
         deferred.resolve([aRv, aZipReader]);
       }
    );

    return deferred.promise;
  },

  _readPackage: function(aOldApp, aNewApp, aIsLocalFileInstall, aIsUpdate,
                         aManifest, aRequestChannel, aHash, aZipReader,
                         aIsSigned) {
    this._checkSignature(aNewApp, aIsSigned, aIsLocalFileInstall);

    if (!aZipReader.hasEntry("manifest.webapp")) {
      throw "MISSING_MANIFEST";
    }

    let istream = aZipReader.getInputStream("manifest.webapp");

    // Obtain a converter to read from a UTF-8 encoded input stream.
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                      .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let newManifest = JSON.parse(converter.ConvertToUnicode(
          NetUtil.readInputStreamToString(istream, istream.available()) || ""));

    if (!AppsUtils.checkManifest(newManifest, aOldApp)) {
      throw "INVALID_MANIFEST";
    }

    // For app updates we don't forbid apps to rename themselves but
    // we still retain the old name of the app. In the future we
    // will use UI to allow updates to rename an app after we check
    // with the user that the rename is ok.
    if (aIsUpdate) {
      // Call ensureSameAppName before compareManifests as `manifest`
      // has been normalized to avoid app rename.
      AppsUtils.ensureSameAppName(aManifest._manifest, newManifest, aOldApp);
    }

    if (!AppsUtils.compareManifests(newManifest, aManifest._manifest)) {
      throw "MANIFEST_MISMATCH";
    }

    if (!AppsUtils.checkInstallAllowed(newManifest, aNewApp.installOrigin)) {
      throw "INSTALL_FROM_DENIED";
    }

    // Local file installs can be privileged even without the signature.
    let maxStatus = aIsSigned || aIsLocalFileInstall
                    ? Ci.nsIPrincipal.APP_STATUS_PRIVILEGED
                    : Ci.nsIPrincipal.APP_STATUS_INSTALLED;

    try {
      // Anything is possible in developer mode.
      if (Services.prefs.getBoolPref("dom.apps.developer_mode")) {
        maxStatus = Ci.nsIPrincipal.APP_STATUS_CERTIFIED;
      }
    } catch(e) {};

    let allowUnsignedLangpack = false;
    try  {
      allowUnsignedLangpack =
        Services.prefs.getBoolPref("dom.apps.allow_unsigned_langpacks") ||
        Services.prefs.getBoolPref("dom.apps.developer_mode");
    } catch(e) {}
    let isLangPack = newManifest.role === "langpack" &&
                     (aIsSigned || allowUnsignedLangpack);

    let status = AppsUtils.getAppManifestStatus(newManifest);
    if (status > maxStatus && !isLangPack) {
      throw "INVALID_SECURITY_LEVEL";
    }

    // Check if the role is allowed for this app.
    if (!AppsUtils.checkAppRole(newManifest.role, status)) {
      throw "INVALID_ROLE";
    }

    this._saveEtag(aIsUpdate, aOldApp, aRequestChannel, aHash, newManifest);
    this._checkOrigin(aIsSigned || aIsLocalFileInstall, aOldApp, newManifest,
                      aIsUpdate);
    this._getIds(aIsSigned, aZipReader, converter, aNewApp, aOldApp, aIsUpdate);

    return newManifest;
  },

  _checkSignature: function(aApp, aIsSigned, aIsLocalFileInstall) {
    // XXX Security: You CANNOT safely add a new app store for
    // installing privileged apps just by modifying this pref and
    // adding the signing cert for that store to the cert trust
    // database. *Any* origin listed can install apps signed with
    // *any* certificate trusted; we don't try to maintain a strong
    // association between certificate with installOrign. The
    // expectation here is that in production builds the pref will
    // contain exactly one origin. However, in custom development
    // builds it may contain more than one origin so we can test
    // different stages (dev, staging, prod) of the same app store.
    //
    // Only allow signed apps to be installed from a whitelist of
    // domains, and require all packages installed from any of the
    // domains on the whitelist to be signed. This is a stopgap until
    // we have a real story for handling multiple app stores signing
    // apps.
    let signedAppOriginsStr =
      Services.prefs.getCharPref("dom.mozApps.signed_apps_installable_from");
    // If it's a local install and it's signed then we assume
    // the app origin is a valid signer.
    let isSignedAppOrigin = (aIsSigned && aIsLocalFileInstall) ||
                             signedAppOriginsStr.split(",").
                                   indexOf(aApp.installOrigin) > -1;
    if (!aIsSigned && isSignedAppOrigin) {
      // Packaged apps installed from these origins must be signed;
      // if not, assume somebody stripped the signature.
      throw "INVALID_SIGNATURE";
    } else if (aIsSigned && !isSignedAppOrigin) {
      // Other origins are *prohibited* from installing signed apps.
      // One reason is that our app revocation mechanism requires
      // strong cooperation from the host of the mini-manifest, which
      // we assume to be under the control of the install origin,
      // even if it has a different origin.
      throw "INSTALL_FROM_DENIED";
    }
  },

  _saveEtag: function(aIsUpdate, aOldApp, aRequestChannel, aHash, aManifest) {
    // Save the new Etag for the package.
    if (aIsUpdate) {
      if (!aOldApp.staged) {
        aOldApp.staged = { };
      }
      try {
        aOldApp.staged.packageEtag = aRequestChannel.getResponseHeader("Etag");
      } catch(e) { }
      aOldApp.staged.packageHash = aHash;
      aOldApp.staged.appStatus = AppsUtils.getAppManifestStatus(aManifest);
    } else {
      try {
        aOldApp.packageEtag = aRequestChannel.getResponseHeader("Etag");
      } catch(e) { }
      aOldApp.packageHash = aHash;
      aOldApp.appStatus = AppsUtils.getAppManifestStatus(aManifest);
    }
  },

  _checkOrigin: function(aIsSigned, aOldApp, aManifest, aIsUpdate) {
    // Check if the app declares which origin it will use.
    if (aIsSigned &&
        aOldApp.appStatus >= Ci.nsIPrincipal.APP_STATUS_PRIVILEGED &&
        aManifest.origin !== undefined) {
      let uri;
      try {
        uri = Services.io.newURI(aManifest.origin, null, null);
      } catch(e) {
        throw "INVALID_ORIGIN";
      }
      if (uri.scheme != "app") {
        throw "INVALID_ORIGIN";
      }

      if (aIsUpdate) {
        // Changing the origin during an update is not allowed.
        if (uri.prePath != aOldApp.origin) {
          throw "INVALID_ORIGIN_CHANGE";
        }
        // Nothing else to do for an update... since the
        // origin can't change we don't need to move the
        // app nor can we have a duplicated origin
      } else {
        debug("Setting origin to " + uri.prePath +
              " for " + aOldApp.manifestURL);
        let newId = uri.prePath.substring(6); // "app://".length
        if (newId in this.webapps && this._isLaunchable(this.webapps[newId])) {
          throw "DUPLICATE_ORIGIN";
        }
        aOldApp.origin = uri.prePath;
        // Update the registry.
        let oldId = aOldApp.id;

        if (oldId == newId) {
          // This could happen when we have an app in the registry
          // that is not launchable. Since the app already has
          // the correct id, we don't need to change it.
          return;
        }

        aOldApp.id = newId;
        this.webapps[newId] = aOldApp;
        delete this.webapps[oldId];
        // Rename the directories where the files are installed.
        [DIRECTORY_NAME, "TmpD"].forEach(function(aDir) {
          let parent = FileUtils.getDir(aDir, ["webapps"], true, true);
          let dir = FileUtils.getDir(aDir, ["webapps", oldId], true, true);
          dir.moveTo(parent, newId);
        });
        // Signals that we need to swap the old id with the new app.
        this.broadcastMessage("Webapps:UpdateApp", { oldId: oldId,
                                                     newId: newId,
                                                     app: aOldApp });

      }
    }
  },

  _getIds: function(aIsSigned, aZipReader, aConverter, aNewApp, aOldApp,
                    aIsUpdate) {
    // Get ids.json if the file is signed
    if (aIsSigned) {
      let idsStream;
      try {
        idsStream = aZipReader.getInputStream("META-INF/ids.json");
      } catch (e) {
        throw aZipReader.hasEntry("META-INF/ids.json")
               ? e
               : "MISSING_IDS_JSON";
      }

      let ids = JSON.parse(aConverter.ConvertToUnicode(NetUtil.
             readInputStreamToString( idsStream, idsStream.available()) || ""));
      if ((!ids.id) || !Number.isInteger(ids.version) ||
          (ids.version <= 0)) {
         throw "INVALID_IDS_JSON";
      }
      let storeId = aNewApp.installOrigin + "#" + ids.id;
      this._checkForStoreIdMatch(aIsUpdate, aOldApp, storeId, ids.version);
      aOldApp.storeId = storeId;
      aOldApp.storeVersion = ids.version;
    }
  },

  // aStoreId must be a string of the form
  //   <installOrigin>#<storeId from ids.json>
  // aStoreVersion must be a positive integer.
  _checkForStoreIdMatch: function(aIsUpdate, aNewApp, aStoreId, aStoreVersion) {
    // Things to check:
    // 1. if it's a update:
    //   a. We should already have this storeId, or the original storeId must
    //      start with STORE_ID_PENDING_PREFIX
    //   b. The manifestURL for the stored app should be the same one we're
    //      updating
    //   c. And finally the version of the update should be higher than the one
    //      on the already installed package
    // 2. else
    //   a. We should not have this storeId on the list
    // We're currently launching WRONG_APP_STORE_ID for all the mismatch kind of
    // errors, and APP_STORE_VERSION_ROLLBACK for the version error.

    // Does an app with this storeID exist already?
    let appId = this.getAppLocalIdByStoreId(aStoreId);
    let isInstalled = appId != Ci.nsIScriptSecurityManager.NO_APP_ID;
    if (aIsUpdate) {
      let isDifferent = aNewApp.localId !== appId;
      let isPending = aNewApp.storeId.indexOf(STORE_ID_PENDING_PREFIX) == 0;

      if ((!isInstalled && !isPending) || (isInstalled && isDifferent)) {
        throw "WRONG_APP_STORE_ID";
      }

      if (!isPending && (aNewApp.storeVersion >= aStoreVersion)) {
        throw "APP_STORE_VERSION_ROLLBACK";
      }

    } else if (isInstalled) {
      throw "WRONG_APP_STORE_ID";
    }
  },

  // Removes the directory we created, and sends an error to the DOM side.
  revertDownloadPackage: function(aId, aOldApp, aNewApp, aIsUpdate, aError) {
    debug("Error downloading package: " + aError);
    let dir = FileUtils.getDir("TmpD", ["webapps", aId], true, true);
    try {
      dir.remove(true);
    } catch (e) { }

    // We avoid notifying the error to the DOM side if the app download
    // was cancelled via cancelDownload, which already sends its own
    // notification.
    if (aOldApp.isCanceling) {
      delete aOldApp.isCanceling;
      return;
    }

    // If the error that got us here was that the package hasn't changed,
    // since we already sent a success and an applied, let's not confuse
    // the clients...
    if (aError == "PACKAGE_UNCHANGED") {
      return;
    }

    let download = AppDownloadManager.get(aNewApp.manifestURL);
    aOldApp.downloading = false;

    // If there were not enough storage to download the package we
    // won't have a record of the download details, so we just set the
    // installState to 'pending' at first download and to 'installed' when
    // updating.
    aOldApp.installState = download ? download.previousState
                                    : aIsUpdate ? "installed"
                                                : "pending";

    // Erase the .staged properties only if there's no download available
    // anymore.
    if (!aOldApp.downloadAvailable && aOldApp.staged) {
      delete aOldApp.staged;
    }

    this._saveApps().then(() => {
      this.broadcastMessage("Webapps:UpdateState", {
        app: aOldApp,
        error: aError,
        id: aId
      });
      this.broadcastMessage("Webapps:FireEvent", {
        eventType: "downloaderror",
        manifestURL:  aNewApp.manifestURL
      });
    });
    AppDownloadManager.remove(aNewApp.manifestURL);
  },

  doUninstall: Task.async(function*(aData, aMm) {
    // The yields here could get stuck forever, so we only hold
    // a weak reference to the message manager while yielding, to avoid
    // leaking the whole page associationed with the message manager.
    aMm = Cu.getWeakReference(aMm);

    let response = "Webapps:Uninstall:Return:OK";

    try {
      aData.app = yield this._getAppWithManifest(aData.manifestURL);

      let prefName = "dom.mozApps.auto_confirm_uninstall";
      if (Services.prefs.prefHasUserValue(prefName) &&
          Services.prefs.getBoolPref(prefName)) {
        yield this._uninstallApp(aData.app);
      } else {
        yield this._promptForUninstall(aData);
      }
    } catch (error) {
      aData.error = error;
      response = "Webapps:Uninstall:Return:KO";
    }

    if ((aMm = aMm.get())) {
      aMm.sendAsyncMessage(response, this.formatMessage(aData));
    }
  }),

  uninstall: function(aManifestURL) {
    return this._getAppWithManifest(aManifestURL)
      .then(this._uninstallApp.bind(this));
  },

  _uninstallApp: Task.async(function*(aApp) {
    if (!aApp.removable) {
      debug("Error: cannot uninstall a non-removable app.");
      throw new Error("NON_REMOVABLE_APP");
    }

    let id = aApp.id;

    // Check if we are downloading something for this app, and cancel the
    // download if needed.
    this.cancelDownload(aApp.manifestURL);

    // Clean up the deprecated manifest cache if needed.
    if (id in this._manifestCache) {
      delete this._manifestCache[id];
    }

    // Clear private data first.
    this._clearPrivateData(aApp.localId, false);

    // Then notify observers.
    Services.obs.notifyObservers(null, "webapps-uninstall", JSON.stringify(aApp));

    if (supportSystemMessages()) {
      this._unregisterActivities(aApp.manifest, aApp);
    }
    UserCustomizations.unregister(aApp.manifest, aApp);
    Langpacks.unregister(aApp, aApp.manifest);

    let dir = this._getAppDir(id);
    try {
      dir.remove(true);
    } catch (e) {}

    delete this.webapps[id];

    yield this._saveApps();

    this.broadcastMessage("Webapps:Uninstall:Broadcast:Return:OK", aApp);
    this.broadcastMessage("Webapps:RemoveApp", { id: id });

    return aApp;
  }),

  _promptForUninstall: function(aData) {
    let deferred = Promise.defer();
    this._pendingUninstalls[aData.requestID] = deferred;
    Services.obs.notifyObservers(null, "webapps-ask-uninstall",
                                 JSON.stringify(aData));
    return deferred.promise;
  },

  confirmUninstall: function(aData) {
    let pending = this._pendingUninstalls[aData.requestID];
    if (pending) {
      delete this._pendingUninstalls[aData.requestID];
      return this._uninstallApp(aData.app).then(() => {
        pending.resolve();
        return aData.app;
      });
    }
    return Promise.reject(new Error("PENDING_UNINSTALL_NOT_FOUND"));
  },

  denyUninstall: function(aData, aReason = "ERROR_UNKNOWN_FAILURE") {
    // Fails to uninstall the desired app because:
    //   - we cannot find the app to be uninstalled.
    //   - the app to be uninstalled is not removable.
    //   - the user declined the confirmation
    debug("Failed to uninstall app: " + aReason);
    let pending = this._pendingUninstalls[aData.requestID];
    if (pending) {
      delete this._pendingUninstalls[aData.requestID];
      pending.reject(new Error(aReason));
      return Promise.resolve();
    }
    return Promise.reject(new Error("PENDING_UNINSTALL_NOT_FOUND"));
  },

  getSelf: function(aData, aMm) {
    aData.apps = [];

    if (aData.appId == Ci.nsIScriptSecurityManager.NO_APP_ID ||
        aData.appId == Ci.nsIScriptSecurityManager.UNKNOWN_APP_ID) {
      aMm.sendAsyncMessage("Webapps:GetSelf:Return:OK", this.formatMessage(aData));
      return;
    }

    let tmp = [];

    for (let id in this.webapps) {
      if (this.webapps[id].origin == aData.origin &&
          this.webapps[id].localId == aData.appId &&
          this._isLaunchable(this.webapps[id])) {
        let app = AppsUtils.cloneAppObject(this.webapps[id]);
        aData.apps.push(app);
        tmp.push({ id: id });
        break;
      }
    }

    if (!aData.apps.length) {
      aMm.sendAsyncMessage("Webapps:GetSelf:Return:OK", this.formatMessage(aData));
      return;
    }

    this._readManifests(tmp).then((aResult) => {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      aMm.sendAsyncMessage("Webapps:GetSelf:Return:OK", this.formatMessage(aData));
    });
  },

  checkInstalled: function(aData, aMm) {
    aData.app = null;
    let tmp = [];

    for (let appId in this.webapps) {
      if (this.webapps[appId].manifestURL == aData.manifestURL &&
          this._isLaunchable(this.webapps[appId])) {
        aData.app = AppsUtils.cloneAppObject(this.webapps[appId]);
        tmp.push({ id: appId });
        break;
      }
    }

    this._readManifests(tmp).then((aResult) => {
      for (let i = 0; i < aResult.length; i++) {
        aData.app.manifest = aResult[i].manifest;
        break;
      }
      aMm.sendAsyncMessage("Webapps:CheckInstalled:Return:OK", this.formatMessage(aData));
    });
  },

  getInstalled: function(aData, aMm) {
    aData.apps = [];
    let tmp = [];

    for (let id in this.webapps) {
      if (this.webapps[id].installOrigin == aData.origin &&
          this._isLaunchable(this.webapps[id])) {
        aData.apps.push(AppsUtils.cloneAppObject(this.webapps[id]));
        tmp.push({ id: id });
      }
    }

    this._readManifests(tmp).then((aResult) => {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      aMm.sendAsyncMessage("Webapps:GetInstalled:Return:OK", this.formatMessage(aData));
    });
  },

  getNotInstalled: function(aData, aMm) {
    aData.apps = [];
    let tmp = [];

    for (let id in this.webapps) {
      if (!this._isLaunchable(this.webapps[id])) {
        aData.apps.push(AppsUtils.cloneAppObject(this.webapps[id]));
        tmp.push({ id: id });
      }
    }

    this._readManifests(tmp).then((aResult) => {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      aMm.sendAsyncMessage("Webapps:GetNotInstalled:Return:OK", this.formatMessage(aData));
    });
  },

  getIcon: function(aData, aMm) {
    let sendError = (aError) => {
      debug("getIcon error: " + aError);
      aData.error = aError;
      aMm.sendAsyncMessage("Webapps:GetIcon:Return", this.formatMessage(aData));
    };

    let app = this.getAppByManifestURL(aData.manifestURL);
    if (!app) {
      sendError("NO_APP");
      return;
    }

    function loadIcon(aUrl) {
      let fallbackMimeType = aUrl.indexOf('.') >= 0 ?
                             "image/" + aUrl.split(".").reverse()[0] : "";
      // Set up an xhr to download a blob.
      let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                  .createInstance(Ci.nsIXMLHttpRequest);
      xhr.mozBackgroundRequest = true;
      xhr.open("GET", aUrl, true);
      xhr.responseType = "blob";
      xhr.addEventListener("load", function() {
        debug("Got http status=" + xhr.status + " for " + aUrl);
        if (xhr.status == 200) {
          let blob = xhr.response;
          // Reusing aData with sendAsyncMessage() leads to an empty blob in
          // the child.
          let payload = {
            "oid": aData.oid,
            "requestID": aData.requestID,
            "blob": blob,
            "type": xhr.getResponseHeader("Content-Type") || fallbackMimeType
          };
          aMm.sendAsyncMessage("Webapps:GetIcon:Return", payload);
        } else if (xhr.status === 0) {
          sendError("NETWORK_ERROR");
        } else {
          sendError("FETCH_ICON_FAILED");
        }
      });
      xhr.addEventListener("error", function() {
        sendError("FETCH_ICON_FAILED");
      });
      xhr.send();
    }

    // Get the manifest, to find the icon url in the current locale.
    this.getManifestFor(aData.manifestURL, aData.entryPoint)
        .then((aManifest) => {
      if (!aManifest) {
        sendError("FETCH_ICON_FAILED");
        return;
      }

      let manifest = new ManifestHelper(aManifest, app.origin, app.manifestURL);
      let url = manifest.iconURLForSize(aData.iconID);
      if (!url) {
        sendError("NO_ICON");
        return;
      }
      loadIcon(url);
    }).catch(() => {
      sendError("FETCH_ICON_FAILED");
      return;
    });
  },

  getAll: function(aCallback) {
    debug("getAll");
    let apps = [];
    let tmp = [];

    for (let id in this.webapps) {
      let app = AppsUtils.cloneAppObject(this.webapps[id]);
      if (!this._isLaunchable(app))
        continue;

      apps.push(app);
      tmp.push({ id: id });
    }

    this._readManifests(tmp).then((aResult) => {
      for (let i = 0; i < aResult.length; i++)
        apps[i].manifest = aResult[i].manifest;
      aCallback(apps);
    });
  },

  /* Check if |data| is actually a receipt */
  isReceipt: function(data) {
    try {
      // The receipt data shouldn't be too big (allow up to 1 MiB of data)
      const MAX_RECEIPT_SIZE = 1048576;

      if (data.length > MAX_RECEIPT_SIZE) {
        return "RECEIPT_TOO_BIG";
      }

      // Marketplace receipts are JWK + "~" + JWT
      // Other receipts may contain only the JWT
      let receiptParts = data.split('~');
      let jwtData = null;
      if (receiptParts.length == 2) {
        jwtData = receiptParts[1];
      } else {
        jwtData = receiptParts[0];
      }

      let segments = jwtData.split('.');
      if (segments.length != 3) {
        return "INVALID_SEGMENTS_NUMBER";
      }

      // We need to translate the base64 alphabet used in JWT to our base64 alphabet
      // before calling atob.
      let decodedReceipt = JSON.parse(atob(segments[1].replace(/-/g, '+')
                                                      .replace(/_/g, '/')));
      if (!decodedReceipt) {
        return "INVALID_RECEIPT_ENCODING";
      }

      // Required values for a receipt
      if (!decodedReceipt.typ) {
        return "RECEIPT_TYPE_REQUIRED";
      }
      if (!decodedReceipt.product) {
        return "RECEIPT_PRODUCT_REQUIRED";
      }
      if (!decodedReceipt.user) {
        return "RECEIPT_USER_REQUIRED";
      }
      if (!decodedReceipt.iss) {
        return "RECEIPT_ISS_REQUIRED";
      }
      if (!decodedReceipt.nbf) {
        return "RECEIPT_NBF_REQUIRED";
      }
      if (!decodedReceipt.iat) {
        return "RECEIPT_IAT_REQUIRED";
      }

      let allowedTypes = [ "purchase-receipt", "developer-receipt",
                           "reviewer-receipt", "test-receipt" ];
      if (allowedTypes.indexOf(decodedReceipt.typ) < 0) {
        return "RECEIPT_TYPE_UNSUPPORTED";
      }
    } catch (e) {
      return "RECEIPT_ERROR";
    }

    return null;
  },

  addReceipt: function(aData, aMm) {
    debug("addReceipt " + aData.manifestURL);

    let receipt = aData.receipt;

    if (!receipt) {
      aData.error = "INVALID_PARAMETERS";
      aMm.sendAsyncMessage("Webapps:AddReceipt:Return:KO", this.formatMessage(aData));
      return;
    }

    let error = this.isReceipt(receipt);
    if (error) {
      aData.error = error;
      aMm.sendAsyncMessage("Webapps:AddReceipt:Return:KO", this.formatMessage(aData));
      return;
    }

    let id = this._appIdForManifestURL(aData.manifestURL);
    let app = this.webapps[id];

    if (!app.receipts) {
      app.receipts = [];
    } else if (app.receipts.length > 500) {
      aData.error = "TOO_MANY_RECEIPTS";
      aMm.sendAsyncMessage("Webapps:AddReceipt:Return:KO", this.formatMessage(aData));
      return;
    }

    let index = app.receipts.indexOf(receipt);
    if (index >= 0) {
      aData.error = "RECEIPT_ALREADY_EXISTS";
      aMm.sendAsyncMessage("Webapps:AddReceipt:Return:KO", this.formatMessage(aData));
      return;
    }

    app.receipts.push(receipt);

    this._saveApps().then(() => {
      aData.receipts = app.receipts;
      aMm.sendAsyncMessage("Webapps:AddReceipt:Return:OK", this.formatMessage(aData));
    });
  },

  removeReceipt: function(aData, aMm) {
    debug("removeReceipt " + aData.manifestURL);

    let receipt = aData.receipt;

    if (!receipt) {
      aData.error = "INVALID_PARAMETERS";
      aMm.sendAsyncMessage("Webapps:RemoveReceipt:Return:KO", this.formatMessage(aData));
      return;
    }

    let id = this._appIdForManifestURL(aData.manifestURL);
    let app = this.webapps[id];

    if (!app.receipts) {
      aData.error = "NO_SUCH_RECEIPT";
      aMm.sendAsyncMessage("Webapps:RemoveReceipt:Return:KO", this.formatMessage(aData));
      return;
    }

    let index = app.receipts.indexOf(receipt);
    if (index == -1) {
      aData.error = "NO_SUCH_RECEIPT";
      aMm.sendAsyncMessage("Webapps:RemoveReceipt:Return:KO", this.formatMessage(aData));
      return;
    }

    app.receipts.splice(index, 1);

    this._saveApps().then(() => {
      aData.receipts = app.receipts;
      aMm.sendAsyncMessage("Webapps:RemoveReceipt:Return:OK", this.formatMessage(aData));
    });
  },

  replaceReceipt: function(aData, aMm) {
    debug("replaceReceipt " + aData.manifestURL);

    let oldReceipt = aData.oldReceipt;
    let newReceipt = aData.newReceipt;

    if (!oldReceipt || !newReceipt) {
      aData.error = "INVALID_PARAMETERS";
      aMm.sendAsyncMessage("Webapps:ReplaceReceipt:Return:KO", this.formatMessage(aData));
      return;
    }

    let error = this.isReceipt(newReceipt);
    if (error) {
      aData.error = error;
      aMm.sendAsyncMessage("Webapps:ReplaceReceipt:Return:KO", this.formatMessage(aData));
      return;
    }

    let id = this._appIdForManifestURL(aData.manifestURL);
    let app = this.webapps[id];

    if (!app.receipts) {
      aData.error = "NO_SUCH_RECEIPT";
      aMm.sendAsyncMessage("Webapps:RemoveReceipt:Return:KO", this.formatMessage(aData));
      return;
    }

    let oldIndex = app.receipts.indexOf(oldReceipt);
    if (oldIndex == -1) {
      aData.error = "NO_SUCH_RECEIPT";
      aMm.sendAsyncMessage("Webapps:ReplaceReceipt:Return:KO", this.formatMessage(aData));
      return;
    }

    app.receipts[oldIndex] = newReceipt;

    this._saveApps().then(() => {
      aData.receipts = app.receipts;
      aMm.sendAsyncMessage("Webapps:ReplaceReceipt:Return:OK", this.formatMessage(aData));
    });
  },

  setEnabled: function(aData) {
    debug("setEnabled " + aData.manifestURL + " : " + aData.enabled);
    let id = this._appIdForManifestURL(aData.manifestURL);
    if (!id || !this.webapps[id]) {
      return;
    }

    debug("Enabling " + id);
    let app = this.webapps[id];
    app.enabled = aData.enabled;
    this._saveApps().then(() => {
      DOMApplicationRegistry.broadcastMessage("Webapps:UpdateState", {
        app: app,
        id: app.id
      });
      this.broadcastMessage("Webapps:SetEnabled:Return", app);
    });

    // Update customization.
    this.getManifestFor(app.manifestURL).then((aManifest) => {
      app.enabled ? UserCustomizations.register(aManifest, app)
                  : UserCustomizations.unregister(aManifest, app);
    });
  },

  getManifestFor: function(aManifestURL, aEntryPoint) {
    let id = this._appIdForManifestURL(aManifestURL);
    let app = this.webapps[id];
    if (!id || (app.installState == "pending" && !app.retryingDownload)) {
      return Promise.resolve(null);
    }

    return this._readManifests([{ id: id }]).then((aResult) => {
      if (aEntryPoint) {
        return aResult[0].manifest.entry_points[aEntryPoint];
      } else {
        return aResult[0].manifest;
      }
    });
  },

  getAppByManifestURL: function(aManifestURL) {
    return AppsUtils.getAppByManifestURL(this.webapps, aManifestURL);
  },

  // Returns a promise that resolves to the app object with the manifest.
  getFullAppByManifestURL: function(aManifestURL, aEntryPoint, aLang) {
    let app = this.getAppByManifestURL(aManifestURL);
    if (!app) {
      return Promise.reject("NoSuchApp");
    }

    return this.getManifestFor(aManifestURL).then((aManifest) => {
      let manifest = aEntryPoint && aManifest.entry_points &&
                     aManifest.entry_points[aEntryPoint]
        ? aManifest.entry_points[aEntryPoint]
        : aManifest;

      // `version` doesn't change based on entry points, and we need it
      // to check langpack versions.
      if (manifest !== aManifest) {
        manifest.version = aManifest.version;
      }

      app.manifest =
        new ManifestHelper(manifest, app.origin, app.manifestURL, aLang);
      return app;
    });
  },

  _getAppWithManifest: Task.async(function*(aManifestURL) {
    let app = this.getAppByManifestURL(aManifestURL);
    if (!app) {
      throw new Error("NO_SUCH_APP");
    }

    app.manifest = ( yield this._readManifests([{ id: app.id }]) )[0].manifest;

    return app;
  }),

  getManifestCSPByLocalId: function(aLocalId) {
    debug("getManifestCSPByLocalId:" + aLocalId);
    return AppsUtils.getManifestCSPByLocalId(this.webapps, aLocalId);
  },

  getDefaultCSPByLocalId: function(aLocalId) {
    debug("getDefaultCSPByLocalId:" + aLocalId);
    return AppsUtils.getDefaultCSPByLocalId(this.webapps, aLocalId);
  },

  getAppLocalIdByStoreId: function(aStoreId) {
    debug("getAppLocalIdByStoreId:" + aStoreId);
    return AppsUtils.getAppLocalIdByStoreId(this.webapps, aStoreId);
  },

  getAppByLocalId: function(aLocalId) {
    return AppsUtils.getAppByLocalId(this.webapps, aLocalId);
  },

  getManifestURLByLocalId: function(aLocalId) {
    return AppsUtils.getManifestURLByLocalId(this.webapps, aLocalId);
  },

  getAppLocalIdByManifestURL: function(aManifestURL) {
    return AppsUtils.getAppLocalIdByManifestURL(this.webapps, aManifestURL);
  },

  getCoreAppsBasePath: function() {
    return AppsUtils.getCoreAppsBasePath();
  },

  getWebAppsBasePath: function() {
    return OS.Path.dirname(this.appsFile);
  },

  _isLaunchable: function(aApp) {
    if (this.allAppsLaunchable)
      return true;

    return WebappOSUtils.isLaunchable(aApp);
  },

  _notifyCategoryAndObservers: function(subject, topic, data,  msg) {
    const serviceMarker = "service,";

    // First create observers from the category manager.
    let cm =
      Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    let enumerator = cm.enumerateCategory(topic);

    let observers = [];

    while (enumerator.hasMoreElements()) {
      let entry =
        enumerator.getNext().QueryInterface(Ci.nsISupportsCString).data;
      let contractID = cm.getCategoryEntry(topic, entry);

      let factoryFunction;
      if (contractID.substring(0, serviceMarker.length) == serviceMarker) {
        contractID = contractID.substring(serviceMarker.length);
        factoryFunction = "getService";
      }
      else {
        factoryFunction = "createInstance";
      }

      try {
        let handler = Cc[contractID][factoryFunction]();
        if (handler) {
          let observer = handler.QueryInterface(Ci.nsIObserver);
          observers.push(observer);
        }
      } catch(e) { }
    }

    // Next enumerate the registered observers.
    enumerator = Services.obs.enumerateObservers(topic);
    while (enumerator.hasMoreElements()) {
      try {
        let observer = enumerator.getNext().QueryInterface(Ci.nsIObserver);
        if (observers.indexOf(observer) == -1) {
          observers.push(observer);
        }
      } catch (e) { }
    }

    observers.forEach(function (observer) {
      try {
        observer.observe(subject, topic, data);
      } catch(e) { }
    });
    // Send back an answer to the child.
    if (msg) {
      ppmm.broadcastAsyncMessage("Webapps:ClearBrowserData:Return", msg);
    }
  },

  registerBrowserElementParentForApp: function(aMsg, aMn) {
    let appId = this.getAppLocalIdByManifestURL(aMsg.manifestURL);
    if (appId == Ci.nsIScriptSecurityManager.NO_APP_ID) {
      return;
    }
    // Make a listener function that holds on to this appId.
    let listener = this.receiveAppMessage.bind(this, appId);

    this.frameMessages.forEach(function(msgName) {
      aMn.addMessageListener(msgName, listener);
    });
  },

  receiveAppMessage: function(appId, message) {
    switch (message.name) {
      case "Webapps:ClearBrowserData":
        this._clearPrivateData(appId, true, message.data);
        break;
    }
  },

  _clearPrivateData: function(appId, browserOnly, msg) {
    let subject = {
      appId: appId,
      browserOnly: browserOnly,
      QueryInterface: XPCOMUtils.generateQI([Ci.mozIApplicationClearPrivateDataParams])
    };
    this._notifyCategoryAndObservers(subject, "webapps-clear-data", null, msg);
  }
};

/**
 * Appcache download observer
 */
let AppcacheObserver = function(aApp) {
  debug("Creating AppcacheObserver for " + aApp.origin +
        " - " + aApp.installState);
  this.app = aApp;
  this.startStatus = aApp.installState;
  this.lastProgressTime = 0;
  // Send a first progress event to correctly set the DOM object's properties.
  this._sendProgressEvent();
};

AppcacheObserver.prototype = {
  // nsIOfflineCacheUpdateObserver implementation
  _sendProgressEvent: function() {
    let app = this.app;
    DOMApplicationRegistry.broadcastMessage("Webapps:UpdateState", {
      app: app,
      id: app.id
    });
    DOMApplicationRegistry.broadcastMessage("Webapps:FireEvent", {
      eventType: "progress",
      manifestURL: app.manifestURL
    });
  },

  updateStateChanged: function appObs_Update(aUpdate, aState) {
    let mustSave = false;
    let app = this.app;

    debug("Offline cache state change for " + app.origin + " : " + aState);

    var self = this;
    let setStatus = function appObs_setStatus(aStatus, aProgress) {
      debug("Offlinecache setStatus to " + aStatus + " with progress " +
            aProgress + " for " + app.origin);
      mustSave = (app.installState != aStatus);

      app.installState = aStatus;
      app.progress = aProgress;
      if (aStatus != "installed") {
        self._sendProgressEvent();
        return;
      }

      app.updateTime = Date.now();
      app.downloading = false;
      app.downloadAvailable = false;
      DOMApplicationRegistry.broadcastMessage("Webapps:UpdateState", {
        app: app,
        id: app.id
      });
      DOMApplicationRegistry.broadcastMessage("Webapps:FireEvent", {
        eventType: ["downloadsuccess", "downloadapplied"],
        manifestURL: app.manifestURL
      });
    }

    let setError = function appObs_setError(aError) {
      debug("Offlinecache setError to " + aError);
      app.downloading = false;
      mustSave = true;

      // If we are canceling the download, we already send a DOWNLOAD_CANCELED
      // error.
      if (app.isCanceling) {
        delete app.isCanceling;
        return;
      }

      DOMApplicationRegistry.broadcastMessage("Webapps:UpdateState", {
        app: app,
        error: aError,
        id: app.id
      });
      DOMApplicationRegistry.broadcastMessage("Webapps:FireEvent", {
        eventType: "downloaderror",
        manifestURL: app.manifestURL
      });
    }

    switch (aState) {
      case Ci.nsIOfflineCacheUpdateObserver.STATE_ERROR:
        aUpdate.removeObserver(this);
        AppDownloadManager.remove(app.manifestURL);
        setError("APP_CACHE_DOWNLOAD_ERROR");
        break;
      case Ci.nsIOfflineCacheUpdateObserver.STATE_NOUPDATE:
      case Ci.nsIOfflineCacheUpdateObserver.STATE_FINISHED:
        aUpdate.removeObserver(this);
        AppDownloadManager.remove(app.manifestURL);
        setStatus("installed", aUpdate.byteProgress);
        break;
      case Ci.nsIOfflineCacheUpdateObserver.STATE_DOWNLOADING:
        setStatus(this.startStatus, aUpdate.byteProgress);
        break;
      case Ci.nsIOfflineCacheUpdateObserver.STATE_ITEMSTARTED:
      case Ci.nsIOfflineCacheUpdateObserver.STATE_ITEMPROGRESS:
        let now = Date.now();
        if (now - this.lastProgressTime > MIN_PROGRESS_EVENT_DELAY) {
          setStatus(this.startStatus, aUpdate.byteProgress);
          this.lastProgressTime = now;
        }
        break;
    }

    // Status changed, update the stored version.
    if (mustSave) {
      DOMApplicationRegistry._saveApps();
    }
  },

  applicationCacheAvailable: function appObs_CacheAvail(aApplicationCache) {
    // Nothing to do.
  }
};

DOMApplicationRegistry.init();
