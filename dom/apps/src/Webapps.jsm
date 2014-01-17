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
Cu.import("resource://gre/modules/PermissionsInstaller.jsm");
Cu.import("resource://gre/modules/OfflineCacheInstaller.jsm");
Cu.import("resource://gre/modules/SystemMessagePermissionsChecker.jsm");
Cu.import("resource://gre/modules/AppDownloadManager.jsm");
Cu.import("resource://gre/modules/WebappOSUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

#ifdef MOZ_WIDGET_GONK
XPCOMUtils.defineLazyGetter(this, "libcutils", function() {
  Cu.import("resource://gre/modules/systemlibs.js");
  return libcutils;
});
#endif

function debug(aMsg) {
#ifdef MOZ_DEBUG
  dump("-*- Webapps.jsm : " + aMsg + "\n");
#endif
}

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

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

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

XPCOMUtils.defineLazyGetter(this, "msgmgr", function() {
  return Cc["@mozilla.org/system-message-internal;1"]
         .getService(Ci.nsISystemMessagesInternal);
});

XPCOMUtils.defineLazyGetter(this, "updateSvc", function() {
  return Cc["@mozilla.org/offlinecacheupdate-service;1"]
           .getService(Ci.nsIOfflineCacheUpdateService);
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
  // Path to the webapps.json file where we store the registry data.
  appsFile: null,
  webapps: { },
  children: [ ],
  allAppsLaunchable: false,

  init: function() {
    this.messages = ["Webapps:Install", "Webapps:Uninstall",
                     "Webapps:GetSelf", "Webapps:CheckInstalled",
                     "Webapps:GetInstalled", "Webapps:GetNotInstalled",
                     "Webapps:Launch", "Webapps:GetAll",
                     "Webapps:InstallPackage",
                     "Webapps:GetList", "Webapps:RegisterForMessages",
                     "Webapps:UnregisterForMessages",
                     "Webapps:CancelDownload", "Webapps:CheckForUpdate",
                     "Webapps:Download", "Webapps:ApplyDownload",
                     "Webapps:Install:Return:Ack",
                     "child-process-shutdown"];

    this.frameMessages = ["Webapps:ClearBrowserData"];

    this.messages.forEach((function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }).bind(this));

    cpmm.addMessageListener("Activities:Register:OK", this);

    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, "memory-pressure", false);

    AppDownloadManager.registerCancelFunction(this.cancelDownload.bind(this));

    this.appsFile = FileUtils.getFile(DIRECTORY_NAME,
                                      ["webapps", "webapps.json"], true).path;

    this.loadAndUpdateApps();
  },

  // loads the current registry, that could be empty on first run.
  loadCurrentRegistry: function() {
    return this._loadJSONAsync(this.appsFile).then((aData) => {
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
        if (this.webapps[id].storeId === undefined) {
          this.webapps[id].storeId = "";
        }
        if (this.webapps[id].storeVersion === undefined) {
          this.webapps[id].storeVersion = 0;
        }

        // Default role to "".
        if (this.webapps[id].role === undefined) {
          this.webapps[id].role = "";
        }

        // At startup we can't be downloading, and the $TMP directory
        // will be empty so we can't just apply a staged update.
        app.downloading = false;
        app.readyToApplyDownload = false;
      }
    });
  },

  // Notify we are starting with registering apps.
  notifyAppsRegistryStart: function notifyAppsRegistryStart() {
    Services.obs.notifyObservers(this, "webapps-registry-start", null);
  },

  // Notify we are done with registering apps and save a copy of the registry.
  notifyAppsRegistryReady: function notifyAppsRegistryReady() {
    Services.obs.notifyObservers(this, "webapps-registry-ready", null);
    this._saveApps();
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

  // Registers all the activities and system messages.
  registerAppsHandlers: function(aRunUpdate) {
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
      this._readManifests(ids).then((aResults) => {
        aResults.forEach((aResult) => {
          if (!aResult.manifest) {
            // If we can't load the manifest, we probably have a corrupted
            // registry. We delete the app since we can't do anything with it.
            delete this.webapps[aResult.id];
            return;
          }
          let app = this.webapps[aResult.id];
          app.csp = aResult.manifest.csp || "";
          app.role = aResult.manifest.role || "";
          if (app.appStatus >= Ci.nsIPrincipal.APP_STATUS_PRIVILEGED) {
            app.redirects = this.sanitizeRedirects(aResult.redirects);
          }
        });
      });

      // Nothing else to do but notifying we're ready.
      this.notifyAppsRegistryReady();
    }
  },

  updateDataStoreForApp: function(aId) {
    if (!this.webapps[aId]) {
      return;
    }

    // Create or Update the DataStore for this app
    this._readManifests([{ id: aId }]).then((aResult) => {
      let app = this.webapps[aId];
      this.updateDataStore(app.localId, app.origin, app.manifestURL,
                           aResult[0].manifest, app.appStatus);
    });
  },

  updatePermissionsForApp: function(aId) {
    if (!this.webapps[aId]) {
      return;
    }

    // Install the permissions for this app, as if we were updating
    // to cleanup the old ones if needed.
    // TODO It's not clear what this should do when there are multiple profiles.
    if (supportUseCurrentProfile()) {
      this._readManifests([{ id: aId }]).then((aResult) => {
        let data = aResult[0];
        PermissionsInstaller.installPermissions({
          manifest: data.manifest,
          manifestURL: this.webapps[aId].manifestURL,
          origin: this.webapps[aId].origin
        }, true, function() {
          debug("Error installing permissions for " + aId);
        });
      });
    }
  },

  updateOfflineCacheForApp: function(aId) {
    let app = this.webapps[aId];
    this._readManifests([{ id: aId }]).then((aResult) => {
      let manifest = new ManifestHelper(aResult[0].manifest, app.origin);
      OfflineCacheInstaller.installCache({
        cachePath: app.cachePath,
        appId: aId,
        origin: Services.io.newURI(app.origin, null, null),
        localId: app.localId,
        appcache_path: manifest.fullAppcachePath()
      });
    });
  },

  // Installs a 3rd party app.
  installPreinstalledApp: function installPreinstalledApp(aId) {
#ifdef MOZ_WIDGET_GONK
    let app = this.webapps[aId];
    let baseDir;
    try {
      baseDir = FileUtils.getDir("coreAppsDir", ["webapps", aId], false);
      if (!baseDir.exists()) {
        return;
      } else if (!baseDir.directoryEntries.hasMoreElements()) {
        debug("Error: Core app in " + baseDir.path + " is empty");
        return;
      }
    } catch(e) {
      // In ENG builds, we don't have apps in coreAppsDir.
      return;
    }

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
        return;
      }

      isPackage = false;
      filesToMove = ["manifest.webapp"];
    } else {
      isPackage = true;
      filesToMove = ["application.zip", "update.webapp"];
    }

    debug("Installing 3rd party app : " + aId +
          " from " + baseDir.path);

    // We copy this app to DIRECTORY_NAME/$aId, and set the base path as needed.
    let destDir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", aId], true, true);

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
      return;
    }

    app.origin = "app://" + aId;

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
      delete this.webapps[aId];
    } finally {
      zipReader.close();
    }
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
         this.uninstall(app.manifestURL, function() {}, function() {});
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
    return Task.spawn(function() {
      let file;
      try {
        file = FileUtils.getFile("coreAppsDir", ["webapps", "webapps.json"], false);
      } catch(e) { }

      if (!file || !file.exists()) {
        return;
      }

      // a
      let data = yield this._loadJSONAsync(file.path);
      if (!data) {
        return;
      }

      // b : core apps are not removable.
      for (let id in this.webapps) {
        if (id in data || this.webapps[id].removable)
          continue;
        // Remove the permissions, cookies and private data for this app.
        let localId = this.webapps[id].localId;
        let permMgr = Cc["@mozilla.org/permissionmanager;1"]
                        .getService(Ci.nsIPermissionManager);
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
        if (!(id in this.webapps)) {
          this.webapps[id] = data[id];
          this.webapps[id].basePath = appDir.path;

          this.webapps[id].id = id;

          // Create a new localId.
          this.webapps[id].localId = this._nextLocalId();

          // Core apps are not removable.
          if (this.webapps[id].removable === undefined) {
            this.webapps[id].removable = false;
          }
        }
      }
    }.bind(this)).then(null, Cu.reportError);
  },

#ifdef MOZ_WIDGET_GONK
  fixIndexedDb: function() {
    debug("Fixing indexedDb folder names");
    let idbDir = FileUtils.getDir("indexedDBPDir", ["indexedDB"]);

    if (!idbDir.exists() || !idbDir.isDirectory()) {
      return;
    }

    let re = /^(\d+)\+(.*)\+(f|t)$/;

    let entries = idbDir.directoryEntries;
    while (entries.hasMoreElements()) {
      let entry = entries.getNext().QueryInterface(Ci.nsIFile);
      if (!entry.isDirectory()) {
        continue;
      }

      let newName = entry.leafName.replace(re, "$1+$3+$2");
      if (newName != entry.leafName) {
        try {
          entry.moveTo(idbDir, newName);
        } catch(e) { }
      }
    }
  },
#endif

  loadAndUpdateApps: function() {
    return Task.spawn(function() {
      let runUpdate = AppsUtils.isFirstRun(Services.prefs);

#ifdef MOZ_WIDGET_GONK
      if (runUpdate) {
        this.fixIndexedDb();
      }
#endif

      yield this.loadCurrentRegistry();

      if (runUpdate) {
#ifdef MOZ_WIDGET_GONK
        yield this.installSystemApps();
#endif

        // At first run, install preloaded apps and set up their permissions.
        for (let id in this.webapps) {
          this.installPreinstalledApp(id);
          this.removeIfHttpsDuplicate(id);
          if (!this.webapps[id]) {
            continue;
          }
          this.updateOfflineCacheForApp(id);
          this.updatePermissionsForApp(id);
        }
        // Need to update the persisted list of apps since
        // installPreinstalledApp() removes the ones failing to install.
        this._saveApps();
      }

      // DataStores must be initialized at startup.
      for (let id in this.webapps) {
        this.updateDataStoreForApp(id);
      }

      this.registerAppsHandlers(runUpdate);
    }.bind(this)).then(null, Cu.reportError);
  },

  updateDataStore: function(aId, aOrigin, aManifestURL, aManifest, aAppStatus) {
    // Just Certified Apps can use DataStores
    let prefName = "dom.testing.datastore_enabled_for_hosted_apps";
    if (aAppStatus != Ci.nsIPrincipal.APP_STATUS_CERTIFIED &&
        (Services.prefs.getPrefType(prefName) == Services.prefs.PREF_INVALID ||
         !Services.prefs.getBoolPref(prefName))) {
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

    let manifest = new ManifestHelper(aManifest, aApp.origin);
    let launchPath = Services.io.newURI(manifest.fullLaunchPath(aEntryPoint), null, null);
    let manifestURL = Services.io.newURI(aApp.manifestURL, null, null);
    root.messages.forEach(function registerPages(aMessage) {
      let href = launchPath;
      let messageName;
      if (typeof(aMessage) === "object" && Object.keys(aMessage).length === 1) {
        messageName = Object.keys(aMessage)[0];
        let uri;
        try {
          uri = manifest.resolveFromOrigin(aMessage[messageName]);
        } catch(e) {
          debug("system message url (" + aMessage[messageName] + ") is invalid, skipping. " +
                "Error is: " + e);
          return;
        }
        href = Services.io.newURI(uri, null, null);
      } else {
        messageName = aMessage;
      }

      if (SystemMessagePermissionsChecker
            .isSystemMessagePermittedToRegister(messageName,
                                                aApp.origin,
                                                aManifest)) {
        msgmgr.registerPage(messageName, href, manifestURL);
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

    let manifest = new ManifestHelper(aManifest, aApp.origin);
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
          fullHandlerPath = manifest.resolveFromOrigin(handlerPath);
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
                                                aApp.origin,
                                                aManifest)) {
        msgmgr.registerPage("connection", handlerPageURI, manifestURI);
      }

      interAppCommService.
        registerConnection(keyword,
                           handlerPageURI,
                           manifestURI,
                           connection.description,
                           AppsUtils.getAppManifestStatus(manifest),
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

    if (!root.activities) {
      return activitiesToRegister;
    }

    let manifest = new ManifestHelper(aManifest, aApp.origin);
    for (let activity in root.activities) {
      let description = root.activities[activity];
      let href = description.href;
      if (!href) {
        href = manifest.launch_path;
      }

      try {
        href = manifest.resolveFromOrigin(href);
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

      let launchPath = Services.io.newURI(href, null, null);
      let manifestURL = Services.io.newURI(aApp.manifestURL, null, null);

      if (SystemMessagePermissionsChecker
            .isSystemMessagePermittedToRegister("activity",
                                                aApp.origin,
                                                aManifest)) {
        msgmgr.registerPage("activity", launchPath, manifestURL);
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
        app.name = manifest.name;
        app.csp = manifest.csp || "";
        app.role = manifest.role || "";
        if (app.appStatus >= Ci.nsIPrincipal.APP_STATUS_PRIVILEGED) {
          app.redirects = this.sanitizeRedirects(manifest.redirects);
        }
        this._registerSystemMessages(manifest, app);
        this._registerInterAppConnections(manifest, app);
        appsToRegister.push({ manifest: manifest, app: app });
      });
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

  _loadJSONAsync: function(aPath) {
    let deferred = Promise.defer();

    try {
      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.initWithPath(aPath);
      let channel = NetUtil.newChannel(file);
      channel.contentType = "application/json";
      NetUtil.asyncFetch(channel, function(aStream, aResult) {
        if (!Components.isSuccessCode(aResult)) {
          Cu.reportError("DOMApplicationRegistry: Could not read from json file "
                         + aPath);
          deferred.resolve(null);
        }

        try {
          // Obtain a converter to read from a UTF-8 encoded input stream.
          let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                          .createInstance(Ci.nsIScriptableUnicodeConverter);
          converter.charset = "UTF-8";

          // Read json file into a string
          let data = JSON.parse(converter.ConvertToUnicode(NetUtil.readInputStreamToString(aStream,
                                                            aStream.available()) || ""));
          aStream.close();

          deferred.resolve(data);
        } catch (ex) {
          Cu.reportError("DOMApplicationRegistry: Could not parse JSON: " +
                         aPath + " " + ex + "\n" + ex.stack);
          deferred.resolve(null);
        }
      });
    } catch (ex) {
      Cu.reportError("DOMApplicationRegistry: Could not read from " +
                     aPath + " : " + ex + "\n" + ex.stack);
      deferred.resolve(null);
    }

    return deferred.promise;
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

  receiveMessage: function(aMessage) {
    // nsIPrefBranch throws if pref does not exist, faster to simply write
    // the pref instead of first checking if it is false.
    Services.prefs.setBoolPref("dom.mozApps.used", true);

    // We need to check permissions for calls coming from mozApps.mgmt.
    // These are: getAll(), getNotInstalled(), applyDownload() and uninstall().
    if (["Webapps:GetAll",
         "Webapps:GetNotInstalled",
         "Webapps:ApplyDownload",
         "Webapps:Uninstall"].indexOf(aMessage.name) != -1) {
      if (!aMessage.target.assertPermission("webapps-manage")) {
        debug("mozApps message " + aMessage.name +
        " from a content process with no 'webapps-manage' privileges.");
        return null;
      }
    }

    let msg = aMessage.data || {};
    let mm = aMessage.target;
    msg.mm = mm;

    switch (aMessage.name) {
      case "Webapps:Install": {
#ifdef MOZ_ANDROID_SYNTHAPKS
        Services.obs.notifyObservers(null, "webapps-download-apk", JSON.stringify(msg));
#else
        this.doInstall(msg, mm);
#endif
        break;
      }
      case "Webapps:GetSelf":
        this.getSelf(msg, mm);
        break;
      case "Webapps:Uninstall":
        this.doUninstall(msg, mm);
        break;
      case "Webapps:Launch":
        this.doLaunch(msg, mm);
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
      case "Webapps:GetAll":
        this.doGetAll(msg, mm);
        break;
      case "Webapps:InstallPackage": {
#ifdef MOZ_ANDROID_SYNTHAPKS
        Services.obs.notifyObservers(null, "webapps-download-apk", JSON.stringify(msg));
#else
        this.doInstallPackage(msg, mm);
#endif
        break;
      }
      case "Webapps:RegisterForMessages":
        this.addMessageListener(msg.messages, msg.app, mm);
        break;
      case "Webapps:UnregisterForMessages":
        this.removeMessageListener(msg, mm);
        break;
      case "child-process-shutdown":
        this.removeMessageListener(["Webapps:Internal:AllMessages"], mm);
        break;
      case "Webapps:GetList":
        this.addMessageListener(["Webapps:AddApp", "Webapps:RemoveApp"], null, mm);
        return this.webapps;
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
      case "Activities:Register:OK":
        this.notifyAppsRegistryReady();
        break;
      case "Webapps:Install:Return:Ack":
        this.onInstallSuccessAck(msg.manifestURL);
        break;
    }
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
    this.children[aMsgName].forEach(function(mmRef) {
      mmRef.mm.sendAsyncMessage(aMsgName, aContent);
    });
  },

  _getAppDir: function(aId) {
    return FileUtils.getDir(DIRECTORY_NAME, ["webapps", aId], true, true);
  },

  _writeFile: function(aPath, aData) {
    debug("Saving " + aPath);

    return OS.File.writeAtomic(aPath,
                               new TextEncoder().encode(aData),
                               { tmpPath: aPath + ".tmp" })
                  .then(null, Cu.reportError);
  },

  doLaunch: function (aData, aMm) {
    this.launch(
      aData.manifestURL,
      aData.startPoint,
      aData.timestamp,
      function onsuccess() {
        aMm.sendAsyncMessage("Webapps:Launch:Return:OK", aData);
      },
      function onfailure(reason) {
        aMm.sendAsyncMessage("Webapps:Launch:Return:KO", aData);
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

    this._saveApps().then(() => {
      this.broadcastMessage("Webapps:UpdateState", {
        app: {
          progress: 0,
          installState: download.previousState,
          downloading: false
        },
        error: error,
        manifestURL: app.manifestURL,
      })
      this.broadcastMessage("Webapps:FireEvent", {
        eventType: "downloaderror",
        manifestURL: app.manifestURL
      });
    });
    AppDownloadManager.remove(aManifestURL);
  },

  startDownload: function startDownload(aManifestURL) {
    debug("startDownload for " + aManifestURL);
    let id = this._appIdForManifestURL(aManifestURL);
    let app = this.webapps[id];
    if (!app) {
      debug("startDownload: No app found for " + aManifestURL);
      return;
    }

    if (app.downloading) {
      debug("app is already downloading. Ignoring.");
      return;
    }

    // If the caller is trying to start a download but we have nothing to
    // download, send an error.
    if (!app.downloadAvailable) {
      this.broadcastMessage("Webapps:UpdateState", {
        error: "NO_DOWNLOAD_AVAILABLE",
        manifestURL: app.manifestURL
      });
      this.broadcastMessage("Webapps:FireEvent", {
        eventType: "downloaderror",
        manifestURL: app.manifestURL
      });
      return;
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
      this._readManifests([{ id: id }]).then((aResults) => {
        let jsonManifest = aResults[0].manifest;
        let manifest = new ManifestHelper(jsonManifest, app.origin);

        if (manifest.appcache_path) {
          debug("appcache found");
          this.startOfflineCacheDownload(manifest, app, null, isUpdate);
        } else {
          // Hosted app with no appcache, nothing to do, but we fire a
          // downloaded event.
          debug("No appcache found, sending 'downloaded' for " + aManifestURL);
          app.downloadAvailable = false;
          this._saveApps().then(() => {
            this.broadcastMessage("Webapps:UpdateState", {
              app: app,
              manifest: jsonManifest,
              manifestURL: aManifestURL
            });
            this.broadcastMessage("Webapps:FireEvent", {
              eventType: "downloadsuccess",
              manifestURL: aManifestURL
            });
          });
        }
      });

      return;
    }

    this._loadJSONAsync(file.path).then((aJSON) => {
      if (!aJSON) {
        debug("startDownload: No update manifest found at " + file.path + " " +
              aManifestURL);
        return;
      }

      let manifest = new ManifestHelper(aJSON, app.installOrigin);
      this.downloadPackage(manifest, {
          manifestURL: aManifestURL,
          origin: app.origin,
          installOrigin: app.installOrigin,
          downloadSize: app.downloadSize
        }, isUpdate).then(function([aId, aManifest]) {
          // Success! Keep the zip in of TmpD, we'll move it out when
          // applyDownload() will be called.
          // Save the manifest in TmpD also
          let manFile = OS.Path.join(OS.Constants.Path.tmpDir, "webapps", aId,
                                     "manifest.webapp");
          DOMApplicationRegistry._writeFile(manFile, JSON.stringify(aManifest));

          app = DOMApplicationRegistry.webapps[aId];
          // Set state and fire events.
          app.downloading = false;
          app.downloadAvailable = false;
          app.readyToApplyDownload = true;
          app.updateTime = Date.now();
          DOMApplicationRegistry._saveApps().then(() => {
            DOMApplicationRegistry.broadcastMessage("Webapps:UpdateState", {
              app: app,
              manifestURL: aManifestURL
            });
            DOMApplicationRegistry.broadcastMessage("Webapps:FireEvent", {
              eventType: "downloadsuccess",
              manifestURL: aManifestURL
            });
            if (app.installState == "pending") {
              // We restarted a failed download, apply it automatically.
              DOMApplicationRegistry.applyDownload(aManifestURL);
            }
          });
        });
    });
  },

  applyDownload: function applyDownload(aManifestURL) {
    debug("applyDownload for " + aManifestURL);
    let id = this._appIdForManifestURL(aManifestURL);
    let app = this.webapps[id];
    if (!app || (app && !app.readyToApplyDownload)) {
      return;
    }

    // We need to get the old manifest to unregister web activities.
    this.getManifestFor(aManifestURL).then((aOldManifest) => {
      // Move the application.zip and manifest.webapp files out of TmpD
      let tmpDir = FileUtils.getDir("TmpD", ["webapps", id], true, true);
      let manFile = tmpDir.clone();
      manFile.append("manifest.webapp");
      let appFile = tmpDir.clone();
      appFile.append("application.zip");

      let dir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", id], true, true);
      appFile.moveTo(dir, "application.zip");
      manFile.moveTo(dir, "manifest.webapp");

      // Move the staged update manifest to a non staged one.
      let staged = dir.clone();
      staged.append("staged-update.webapp");

      // If we are applying after a restarted download, we have no
      // staged update manifest.
      if (staged.exists()) {
        staged.moveTo(dir, "update.webapp");
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
      this.getManifestFor(aManifestURL).then((aData) => {
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

        this._saveApps().then(() => {
          // Update the handlers and permissions for this app.
          this.updateAppHandlers(aOldManifest, aData, app);
          if (supportUseCurrentProfile()) {
            PermissionsInstaller.installPermissions(
              { manifest: aData,
                origin: app.origin,
                manifestURL: app.manifestURL },
              true);
          }
          this.updateDataStore(this.webapps[id].localId, app.origin,
                               app.manifestURL, aData, app.appStatus);
          this.broadcastMessage("Webapps:UpdateState", {
            app: app,
            manifest: aData,
            manifestURL: app.manifestURL
          });
          this.broadcastMessage("Webapps:FireEvent", {
            eventType: "downloadapplied",
            manifestURL: app.manifestURL
          });
        });
      });
    });
  },

  startOfflineCacheDownload: function(aManifest, aApp, aProfileDir, aIsUpdate) {
    if (!aManifest.appcache_path) {
      return;
    }

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
        app: {
          downloading: true,
          installState: aApp.installState,
          progress: 0
        },
        manifestURL: aApp.manifestURL
      });
      let cacheUpdate = aProfileDir
        ? updateSvc.scheduleCustomProfileUpdate(appcacheURI, docURI, aProfileDir)
        : updateSvc.scheduleAppUpdate(appcacheURI, docURI, aApp.localId, false);

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
    debug("updateAppHandlers: old=" + aOldManifest + " new=" + aNewManifest);
    this.notifyAppsRegistryStart();
    if (aApp.appStatus >= Ci.nsIPrincipal.APP_STATUS_PRIVILEGED) {
      aApp.redirects = this.sanitizeRedirects(aNewManifest.redirects);
    }

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
  },

  checkForUpdate: function(aData, aMm) {
    debug("checkForUpdate for " + aData.manifestURL);

    function sendError(aError) {
      aData.error = aError;
      aMm.sendAsyncMessage("Webapps:CheckForUpdate:Return:KO", aData);
    }

    let id = this._appIdForManifestURL(aData.manifestURL);
    let app = this.webapps[id];

    function updatePackagedApp(aManifest) {
      debug("updatePackagedApp");

      // if the app manifestURL has a app:// scheme, we can't have an
      // update.
      if (app.manifestURL.startsWith("app://")) {
        aMm.sendAsyncMessage("Webapps:CheckForUpdate:Return:KO", aData);
        return;
      }

      // Store the new update manifest.
      let dir = this._getAppDir(id).path;
      let manFile = OS.Path.join(dir, "staged-update.webapp");
      this._writeFile(manFile, JSON.stringify(aManifest));

      let manifest = new ManifestHelper(aManifest, app.manifestURL);
      // A package is available: set downloadAvailable to fire the matching
      // event.
      app.downloadAvailable = true;
      app.downloadSize = manifest.size;
      app.updateManifest = aManifest;
      this._saveApps().then(() => {
        this.broadcastMessage("Webapps:UpdateState", {
          app: app,
          manifestURL: app.manifestURL
        });
        this.broadcastMessage("Webapps:FireEvent", {
          eventType: "downloadavailable",
          manifestURL: app.manifestURL,
          requestID: aData.requestID
        });
      });
    }

    // A hosted app is updated if the app manifest or the appcache needs
    // updating. Even if the app manifest has not changed, we still check
    // for changes in the app cache.
    // 'aNewManifest' would contain the updated app manifest if
    // it has actually been updated, while 'aOldManifest' contains the
    // stored app manifest.
    function updateHostedApp(aOldManifest, aNewManifest) {
      debug("updateHostedApp " + aData.manifestURL);

      // Clean up the deprecated manifest cache if needed.
      if (id in this._manifestCache) {
        delete this._manifestCache[id];
      }

      app.manifest = aNewManifest || aOldManifest;

      let manifest;
      if (aNewManifest) {
        this.updateAppHandlers(aOldManifest, aNewManifest, app);

        // Store the new manifest.
        let dir = this._getAppDir(id).path;
        let manFile = OS.Path.join(dir, "manifest.webapp");
        this._writeFile(manFile, JSON.stringify(aNewManifest));
        manifest = new ManifestHelper(aNewManifest, app.origin);

        if (supportUseCurrentProfile()) {
          // Update the permissions for this app.
          PermissionsInstaller.installPermissions({
            manifest: app.manifest,
            origin: app.origin,
            manifestURL: aData.manifestURL
          }, true);
        }

        this.updateDataStore(this.webapps[id].localId, app.origin,
                             app.manifestURL, app.manifest, app.appStatus);

        app.name = manifest.name;
        app.csp = manifest.csp || "";
        app.role = manifest.role || "";
        app.updateTime = Date.now();
      } else {
        manifest = new ManifestHelper(aOldManifest, app.origin);
      }

      // Update the registry.
      this.webapps[id] = app;
      this._saveApps().then(() => {
        let reg = DOMApplicationRegistry;
        if (!manifest.appcache_path) {
          reg.broadcastMessage("Webapps:UpdateState", {
            app: app,
            manifest: app.manifest,
            manifestURL: app.manifestURL
          });
          reg.broadcastMessage("Webapps:FireEvent", {
            eventType: "downloadapplied",
            manifestURL: app.manifestURL,
            requestID: aData.requestID
          });
        } else {
          // Check if the appcache is updatable, and send "downloadavailable" or
          // "downloadapplied".
          let updateObserver = {
            observe: function(aSubject, aTopic, aObsData) {
              debug("updateHostedApp: updateSvc.checkForUpdate return for " +
                    app.manifestURL + " - event is " + aTopic);
              let eventType =
                aTopic == "offline-cache-update-available" ? "downloadavailable"
                                                           : "downloadapplied";
              app.downloadAvailable = (eventType == "downloadavailable");
              reg._saveApps().then(() => {
                reg.broadcastMessage("Webapps:UpdateState", {
                  app: app,
                  manifest: app.manifest,
                  manifestURL: app.manifestURL
                });
                reg.broadcastMessage("Webapps:FireEvent", {
                  eventType: eventType,
                  manifestURL: app.manifestURL,
                  requestID: aData.requestID
                });
              });
            }
          };
          debug("updateHostedApp: updateSvc.checkForUpdate for " +
                manifest.fullAppcachePath());
          updateSvc.checkForUpdate(Services.io.newURI(manifest.fullAppcachePath(), null, null),
                                   app.localId, false, updateObserver);
        }
        delete app.manifest;
      });
    }


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

    // For non-removable hosted apps that lives in the core apps dir we
    // only check the appcache because we can't modify the manifest even
    // if it has changed.
    let onlyCheckAppCache = false;

#ifdef MOZ_WIDGET_GONK
    let appDir = FileUtils.getDir("coreAppsDir", ["webapps"], false);
    onlyCheckAppCache = (app.basePath == appDir.path);
#endif

    if (onlyCheckAppCache) {
      // Bail out for packaged apps.
      if (app.origin.startsWith("app://")) {
        aData.error = "NOT_UPDATABLE";
        aMm.sendAsyncMessage("Webapps:CheckForUpdate:Return:KO", aData);
        return;
      }

      // We need the manifest to check if we have an appcache.
      this._readManifests([{ id: id }]).then((aResult) => {
        let manifest = aResult[0].manifest;
        if (!manifest.appcache_path) {
          aData.error = "NOT_UPDATABLE";
          aMm.sendAsyncMessage("Webapps:CheckForUpdate:Return:KO", aData);
          return;
        }

        debug("Checking only appcache for " + aData.manifestURL);
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
                  manifestURL: app.manifestURL
                });
                this.broadcastMessage("Webapps:FireEvent", {
                  eventType: "downloadavailable",
                  manifestURL: app.manifestURL,
                  requestID: aData.requestID
                });
              });
            } else {
              aData.error = "NOT_UPDATABLE";
              aMm.sendAsyncMessage("Webapps:CheckForUpdate:Return:KO", aData);
            }
          }
        };
        let helper = new ManifestHelper(manifest);
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
      let isPackage = app.origin.startsWith("app://");

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
              updatePackagedApp.call(this, manifest);
            } else {
              this._saveApps().then(() => {
                // Like if we got a 304, just send a 'downloadapplied'
                // or downloadavailable event.
                let eventType = app.downloadAvailable ? "downloadavailable"
                                                      : "downloadapplied";
                aMm.sendAsyncMessage("Webapps:UpdateState", {
                  app: app,
                  manifestURL: app.manifestURL
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
            updateHostedApp.call(this, oldManifest,
                                 oldHash == hash ? null : manifest);
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
              manifestURL: app.manifestURL
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
          updateHostedApp.call(this, oldManifest, null);
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
        this.createLoadContext(app.installerAppId, app.installerIsBrowser);

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

  // Creates a nsILoadContext object with a given appId and isBrowser flag.
  createLoadContext: function createLoadContext(aAppId, aIsBrowser) {
    return {
       associatedWindow: null,
       topWindow : null,
       appId: aAppId,
       isInBrowserElement: aIsBrowser,
       usePrivateBrowsing: false,
       isContent: false,

       isAppOfType: function(appType) {
         throw Cr.NS_ERROR_NOT_IMPLEMENTED;
       },

       QueryInterface: XPCOMUtils.generateQI([Ci.nsILoadContext,
                                              Ci.nsIInterfaceRequestor,
                                              Ci.nsISupports]),
       getInterface: function(iid) {
         if (iid.equals(Ci.nsILoadContext))
           return this;
         throw Cr.NS_ERROR_NO_INTERFACE;
       }
     }
  },

  // Downloads the manifest and run checks, then eventually triggers the
  // installation UI.
  doInstall: function doInstall(aData, aMm) {
    let app = aData.app;

    let sendError = function sendError(aError) {
      aData.error = aError;
      aMm.sendAsyncMessage("Webapps:Install:Return:KO", aData);
      Cu.reportError("Error installing app from: " + app.installOrigin +
                     ": " + aError);
    }.bind(this);

    // Hosted apps can't be trusted or certified, so just check that the
    // manifest doesn't ask for those.
    function checkAppStatus(aManifest) {
      let manifestStatus = aManifest.type || "web";
      return manifestStatus === "web";
    }

    let checkManifest = (function() {
      if (!app.manifest) {
        sendError("MANIFEST_PARSE_ERROR");
        return false;
      }

      // Disallow multiple hosted apps installations from the same origin for now.
      // We will remove this code after multiple apps per origin are supported (bug 778277).
      // This will also disallow reinstalls from the same origin for now.
      for (let id in this.webapps) {
        if (this.webapps[id].origin == app.origin &&
            !this.webapps[id].packageHash &&
            this._isLaunchable(this.webapps[id])) {
          sendError("MULTIPLE_APPS_PER_ORIGIN_FORBIDDEN");
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

      return true;
    }).bind(this);

    let installApp = (function() {
      app.manifestHash = this.computeManifestHash(app.manifest);
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
    }).bind(this);

    // We may already have the manifest (e.g. AutoInstall),
    // in which case we don't need to load it.
    if (app.manifest) {
      if (checkManifest()) {
        installApp();
      }
      return;
    }

    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", app.manifestURL, true);
    xhr.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    xhr.channel.notificationCallbacks = this.createLoadContext(aData.appId,
                                                               aData.isBrowser);
    xhr.responseType = "json";

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200) {
        if (!AppsUtils.checkManifestContentType(app.installOrigin, app.origin,
                                                xhr.getResponseHeader("content-type"))) {
          sendError("INVALID_MANIFEST");
          return;
        }

        app.manifest = xhr.response;
        if (checkManifest()) {
          app.etag = xhr.getResponseHeader("Etag");
          installApp();
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

    let sendError = function sendError(aError) {
      aData.error = aError;
      aMm.sendAsyncMessage("Webapps:Install:Return:KO", aData);
      Cu.reportError("Error installing packaged app from: " +
                     app.installOrigin + ": " + aError);
    }.bind(this);

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
    }).bind(this);

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
    xhr.channel.notificationCallbacks = this.createLoadContext(aData.appId,
                                                               aData.isBrowser);
    xhr.responseType = "json";

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200) {
        if (!AppsUtils.checkManifestContentType(app.installOrigin, app.origin,
                                                xhr.getResponseHeader("content-type"))) {
          sendError("INVALID_MANIFEST");
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
    aData.mm.sendAsyncMessage("Webapps:Install:Return:KO", aData);
  },

  // This function is called after we called the onsuccess callback on the
  // content side. This let the webpage the opportunity to set event handlers
  // on the app before we start firing progress events.
  queuedDownload: {},
  queuedPackageDownload: {},

onInstallSuccessAck: function onInstallSuccessAck(aManifestURL,
                                                  aDontNeedNetwork) {
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

      this.downloadPackage(manifest, newApp, false).then(
        this._onDownloadPackage.bind(this, newApp, installSuccessCallback)
      );
    }
  },

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

  _cloneApp: function(aData, aNewApp, aManifest, aId, aLocalId) {
    let appObject = AppsUtils.cloneAppObject(aNewApp);
    appObject.appStatus =
      aNewApp.appStatus || Ci.nsIPrincipal.APP_STATUS_INSTALLED;

    if (aManifest.appcache_path) {
      appObject.installState = "pending";
      appObject.downloadAvailable = true;
      appObject.downloading = true;
      appObject.downloadSize = 0;
      appObject.readyToApplyDownload = false;
    } else if (aManifest.package_path) {
      appObject.installState = "pending";
      appObject.downloadAvailable = true;
      appObject.downloading = true;
      appObject.downloadSize = aManifest.size;
      appObject.readyToApplyDownload = false;
    } else {
      appObject.installState = "installed";
      appObject.downloadAvailable = false;
      appObject.downloading = false;
      appObject.readyToApplyDownload = false;
    }

    appObject.localId = aLocalId;
    appObject.basePath = OS.Path.dirname(this.appsFile);
    appObject.name = aManifest.name;
    appObject.csp = aManifest.csp || "";
    appObject.role = aManifest.role || "";
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
    this._writeFile(manFile, JSON.stringify(aJsonManifest));
  },

  confirmInstall: function(aData, aProfileDir, aInstallSuccessCallback) {
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
    this._writeManifestFile(id, aData.isPackage, jsonManifest);

    debug("app.origin: " + app.origin);
    let manifest = new ManifestHelper(jsonManifest, app.origin);

    let appObject = this._cloneApp(aData, app, manifest, id, localId);

    this.webapps[id] = appObject;

    // For package apps, the permissions are not in the mini-manifest, so
    // don't update the permissions yet.
    if (!aData.isPackage) {
      if (supportUseCurrentProfile()) {
        PermissionsInstaller.installPermissions(
          {
            origin: appObject.origin,
            manifestURL: appObject.manifestURL,
            manifest: jsonManifest
          },
          isReinstall,
          this.uninstall.bind(this, aData, aData.mm)
        );
      }

      this.updateDataStore(this.webapps[id].localId,  this.webapps[id].origin,
                           this.webapps[id].manifestURL, jsonManifest,
                           this.webapps[id].appStatus);
    }

    for each (let prop in ["installState", "downloadAvailable", "downloading",
                           "downloadSize", "readyToApplyDownload"]) {
      aData.app[prop] = appObject[prop];
    }

    if (manifest.appcache_path) {
      this.queuedDownload[app.manifestURL] = {
        manifest: manifest,
        app: appObject,
        profileDir: aProfileDir
      }
    }

    // We notify about the successful installation via mgmt.oninstall and the
    // corresponging DOMRequest.onsuccess event as soon as the app is properly
    // saved in the registry.
    this._saveApps().then(() => {
      this.broadcastMessage("Webapps:AddApp", { id: id, app: appObject });
      if (aData.isPackage && aData.autoInstall) {
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
    });

    if (!aData.isPackage) {
      this.updateAppHandlers(null, app.manifest, app);
      if (aInstallSuccessCallback) {
        aInstallSuccessCallback(app.manifest);
      }
    }
    let dontNeedNetwork = false;
    if (manifest.package_path) {
      // If it is a local app then it must been installed from a local file
      // instead of web.
#ifdef MOZ_ANDROID_SYNTHAPKS
      // In that case, we would already have the manifest, not just the update
      // manifest.
      dontNeedNetwork = !!aData.app.manifest;
#else
      if (aData.app.localInstallPath) {
        dontNeedNetwork = true;
        jsonManifest.package_path = "file://" + aData.app.localInstallPath;
      }
#endif

      // origin for install apps is meaningless here, since it's app:// and this
      // can't be used to resolve package paths.
      manifest = new ManifestHelper(jsonManifest, app.manifestURL);

      this.queuedPackageDownload[app.manifestURL] = {
        manifest: manifest,
        app: appObject,
        callback: aInstallSuccessCallback
      };
    }

    if (aData.forceSuccessAck) {
      // If it's a local install, there's no content process so just
      // ack the install.
      this.onInstallSuccessAck(app.manifestURL, dontNeedNetwork);
    }
  },

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
  _onDownloadPackage: function(aNewApp, aInstallSuccessCallback,
                               [aId, aManifest]) {
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
    this._writeFile(manFile, JSON.stringify(aManifest));
    // Set state and fire events.
    app.installState = "installed";
    app.downloading = false;
    app.downloadAvailable = false;
    this._saveApps().then(() => {
      this.updateAppHandlers(null, aManifest, aNewApp);
      this.broadcastMessage("Webapps:AddApp", { id: aId, app: aNewApp });
      Services.obs.notifyObservers(null, "webapps-installed",
        JSON.stringify({ manifestURL: aNewApp.manifestURL }));

      if (supportUseCurrentProfile()) {
        // Update the permissions for this app.
        PermissionsInstaller.installPermissions({
          manifest: aManifest,
          origin: aNewApp.origin,
          manifestURL: aNewApp.manifestURL
        }, true);
      }

      this.updateDataStore(this.webapps[aId].localId, aNewApp.origin,
                           aNewApp.manifestURL, aManifest, aNewApp.appStatus);

      this.broadcastMessage("Webapps:UpdateState", {
        app: app,
        manifest: aManifest,
        manifestURL: aNewApp.manifestURL
      });
      this.broadcastMessage("Webapps:FireEvent", {
        eventType: ["downloadsuccess", "downloadapplied"],
        manifestURL: aNewApp.manifestURL
      });
      if (aInstallSuccessCallback) {
        aInstallSuccessCallback(aManifest, zipFile.path);
      }
    });
  },

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
    return Task.spawn(function*() {
      if (!aData.length) {
        return aData;
      }

      for (let elem of aData) {
        let id = elem.id;

        if (!this._manifestCache[id]) {
          // the manifest file used to be named manifest.json, so fallback on this.
          let baseDir = this.webapps[id].basePath == this.getCoreAppsBasePath()
                          ? "coreAppsDir" : DIRECTORY_NAME;

          let file = FileUtils.getFile(baseDir, ["webapps", id, "manifest.webapp"], true);

          if (!file.exists()) {
            file = FileUtils.getFile(baseDir, ["webapps", id, "update.webapp"], true);
          }

          if (!file.exists()) {
            file = FileUtils.getFile(baseDir, ["webapps", id, "manifest.json"], true);
          }

          this._manifestCache[id] = yield this._loadJSONAsync(file.path);
        }

        elem.manifest = this._manifestCache[id];
      }

      return aData;
    }.bind(this)).then(null, Cu.reportError);
  },

  downloadPackage: function(aManifest, aNewApp, aIsUpdate, aOnSuccess) {
    // Here are the steps when installing a package:
    // - create a temp directory where to store the app.
    // - download the zip in this directory.
    // - check the signature on the zip.
    // - extract the manifest from the zip and check it.
    // - ask confirmation to the user.
    // - add the new app to the registry.
    // If we fail at any step, we revert the previous ones and return an error.

    // We define these outside the task to use them in its reject handler.
    let id = this._appIdForManifestURL(aNewApp.manifestURL);
    let oldApp = this.webapps[id];

    return Task.spawn((function*() {
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
                                                   oldApp,
                                                   aNewApp);

      AppDownloadManager.add(
        aNewApp.manifestURL,
        {
          channel: requestChannel,
          appId: id,
          previousState: aIsUpdate ? "installed" : "pending"
        }
      );

      // We set the 'downloading' flag to true right before starting the fetch.
      oldApp.downloading = true;

      // We determine the app's 'installState' according to its previous
      // state. Cancelled download should remain as 'pending'. Successfully
      // installed apps should morph to 'updating'.
      oldApp.installState = aIsUpdate ? "updating" : "pending";

      // initialize the progress to 0 right now
      oldApp.progress = 0;

      let zipFile = yield this._getPackage(requestChannel, id, oldApp, aNewApp);
      let hash = yield this._computeFileHash(zipFile.path);

      let responseStatus = requestChannel.responseStatus;
      let oldPackage = (responseStatus == 304 || hash == oldApp.packageHash);

      if (oldPackage) {
        debug("package's etag or hash unchanged; sending 'applied' event");
        // The package's Etag or hash has not changed.
        // We send a "applied" event right away.
        this._sendAppliedEvent(aNewApp, oldApp, id);
        return;
      }

      let newManifest = yield this._openAndReadPackage(zipFile, oldApp, aNewApp,
              isLocalFileInstall, aIsUpdate, aManifest, requestChannel, hash);

      AppDownloadManager.remove(aNewApp.manifestURL);

      return [id, newManifest];

    }).bind(this)).then(
      aOnSuccess,
      this._revertDownloadPackage.bind(this, id, oldApp, aNewApp, aIsUpdate)
    );
  },

  _ensureSufficientStorage: function(aNewApp) {
    let deferred = Promise.defer();

    let navigator = Services.wm.getMostRecentWindow("navigator:browser")
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

    if (aIsLocalFileInstall) {
      requestChannel = NetUtil.newChannel(aFullPackagePath)
                              .QueryInterface(Ci.nsIFileChannel);
    } else {
      requestChannel = NetUtil.newChannel(aFullPackagePath)
                              .QueryInterface(Ci.nsIHttpChannel);
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
      manifestURL: aNewApp.manifestURL
    });
    this.broadcastMessage("Webapps:FireEvent", {
      eventType: "progress",
      manifestURL: aNewApp.manifestURL
    });
  },

  _getPackage: function(aRequestChannel, aId, aOldApp, aNewApp) {
    let deferred = Promise.defer();

    // Staging the zip in TmpD until all the checks are done.
    let zipFile =
      FileUtils.getFile("TmpD", ["webapps", aId, "application.zip"], true);

    // We need an output stream to write the channel content to the zip file.
    let outputStream = Cc["@mozilla.org/network/file-output-stream;1"]
                         .createInstance(Ci.nsIFileOutputStream);
    // write, create, truncate
    outputStream.init(zipFile, 0x02 | 0x08 | 0x20, parseInt("0664", 8), 0);
    let bufferedOutputStream =
      Cc['@mozilla.org/network/buffered-output-stream;1']
        .createInstance(Ci.nsIBufferedOutputStream);
    bufferedOutputStream.init(outputStream, 1024);

    // Create a listener that will give data to the file output stream.
    let listener = Cc["@mozilla.org/network/simple-stream-listener;1"]
                     .createInstance(Ci.nsISimpleStreamListener);

    listener.init(bufferedOutputStream, {
      onStartRequest: function(aRequest, aContext) {
        // Nothing to do there anymore.
      },

      onStopRequest: function(aRequest, aContext, aStatusCode) {
        bufferedOutputStream.close();
        outputStream.close();

        if (!Components.isSuccessCode(aStatusCode)) {
          deferred.reject("NETWORK_ERROR");
          return;
        }

        // If we get a 4XX or a 5XX http status, bail out like if we had a
        // network error.
        let responseStatus = aRequestChannel.responseStatus;
        if (responseStatus >= 400 && responseStatus <= 599) {
          // unrecoverable error, don't bug the user
          aOldApp.downloadAvailable = false;
          deferred.reject("NETWORK_ERROR");
          return;
        }

        deferred.resolve(zipFile);
      }
    });
    aRequestChannel.asyncOpen(listener, null);

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
    return Task.spawn(function*() {
      let hasher = Cc["@mozilla.org/security/hash;1"]
                     .createInstance(Ci.nsICryptoHash);
      // We want to use the MD5 algorithm.
      hasher.init(hasher.MD5);

      const CHUNK_SIZE = 16384;

      // Return the two-digit hexadecimal code for a byte.
      function toHexString(charCode) {
        return ("0" + charCode.toString(16)).slice(-2);
      }

      let file;
      try {
        file = yield OS.File.open(aFilePath, { read: true });
      } catch(e) {
        debug("Error opening " + aFilePath + ": " + e);
        return null;
      }

      try {
        let array;
        do {
          array = yield file.read(CHUNK_SIZE);
          hasher.update(array, array.length);
        } while (array.length == CHUNK_SIZE);
      } catch(e) {
        debug("Error reading " + aFilePath + ": " + e);
        return null;
      }

      yield file.close();

      // We're passing false to get the binary hash and not base64.
      let data = hasher.finish(false);
      // Convert the binary hash data to a hex string.
      let hash = [toHexString(data.charCodeAt(i)) for (i in data)].join("");
      debug("File hash computed: " + hash);

      return hash;
    });
  },

  /**
   * Send an "applied" event right away for the package being installed.
   *
   * XXX We use this to exit the app update process early when the downloaded
   * package is identical to the last one we installed.  Presumably we do
   * something similar after updating the app, and we could refactor both cases
   * to use the same code to send the "applied" event.
   *
   * @param aNewApp {Object} the new app data
   * @param aOldApp {Object} the currently stored app data
   * @param aId {String} the unique id of the app
   */
  _sendAppliedEvent: function(aNewApp, aOldApp, aId) {
    aOldApp.downloading = false;
    aOldApp.downloadAvailable = false;
    aOldApp.downloadSize = 0;
    aOldApp.installState = "installed";
    aOldApp.readyToApplyDownload = false;
    if (aOldApp.staged && aOldApp.staged.manifestHash) {
      // If we're here then the manifest has changed but the package
      // hasn't. Let's clear this, so we don't keep offering
      // a bogus update to the user
      aOldApp.manifestHash = aOldApp.staged.manifestHash;
      aOldApp.etag = aOldApp.staged.etag || aOldApp.etag;
      aOldApp.staged = {};
      // Move the staged update manifest to a non staged one.
      let dirPath = this._getAppDir(aId).path;

      // We don't really mind much if this fails.
      OS.File.move(OS.Path.join(dirPath, "staged-update.webapp"),
                   OS.Path.join(dirPath, "update.webapp"));
    }

    // Save the updated registry, and cleanup the tmp directory.
    this._saveApps().then(() => {
      this.broadcastMessage("Webapps:UpdateState", {
        app: aOldApp,
        manifestURL: aNewApp.manifestURL
      });
      this.broadcastMessage("Webapps:FireEvent", {
        manifestURL: aNewApp.manifestURL,
        eventType: ["downloadsuccess", "downloadapplied"]
      });
    });
    let file = FileUtils.getFile("TmpD", ["webapps", aId], false);
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
          Cu.reportError("Error while reading package:" + e);
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

      let [result, zipReader] = yield this._openSignedPackage(aZipFile, certDb);

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

  _openSignedPackage: function(aZipFile, aCertDb) {
    let deferred = Promise.defer();

    aCertDb.openSignedJARFileAsync(
       aZipFile,
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

    if (AppsUtils.getAppManifestStatus(newManifest) > maxStatus) {
      throw "INVALID_SECURITY_LEVEL";
    }

    aOldApp.appStatus = AppsUtils.getAppManifestStatus(newManifest);

    this._saveEtag(aIsUpdate, aOldApp, aRequestChannel, aHash, newManifest);
    this._checkOrigin(aIsSigned, aOldApp, newManifest, aIsUpdate);
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
        if (uri.prePath != app.origin) {
          throw "INVALID_ORIGIN_CHANGE";
        }
        // Nothing else to do for an update... since the
        // origin can't change we don't need to move the
        // app nor can we have a duplicated origin
      } else {
        debug("Setting origin to " + uri.prePath +
              " for " + app.manifestURL);
        let newId = uri.prePath.substring(6); // "app://".length
        if (newId in this.webapps) {
          throw "DUPLICATE_ORIGIN";
        }
        aOldApp.origin = uri.prePath;
        // Update the registry.
        aOldApp.id = newId;
        this.webapps[newId] = aOldApp;
        delete this.webapps[aId];
        // Rename the directories where the files are installed.
        [DIRECTORY_NAME, "TmpD"].forEach(function(aDir) {
          let parent = FileUtils.getDir(aDir, ["webapps"], true, true);
          let dir = FileUtils.getDir(aDir, ["webapps", aId], true, true);
          dir.moveTo(parent, newId);
        });
        // Signals that we need to swap the old id with the new app.
        this.broadcastMessage("Webapps:RemoveApp", { id: aId });
        this.broadcastMessage("Webapps:AddApp", { id: newId,
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
  _revertDownloadPackage: function(aId, aOldApp, aNewApp, aIsUpdate, aError) {
    debug("Cleanup: " + aError + "\n" + aError.stack);
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

    let download = AppDownloadManager.get(aNewApp.manifestURL);
    aOldApp.downloading = false;

    // If there were not enough storage to download the package we
    // won't have a record of the download details, so we just set the
    // installState to 'pending' at first download and to 'installed' when
    // updating.
    aOldApp.installState = download ? download.previousState
                                    : aIsUpdate ? "installed"
                                                : "pending";

    if (aOldApp.staged) {
      delete aOldApp.staged;
    }

    this._saveApps().then(() => {
      this.broadcastMessage("Webapps:UpdateState", {
        app: aOldApp,
        error: aError,
        manifestURL: aNewApp.manifestURL
      });
      this.broadcastMessage("Webapps:FireEvent", {
        eventType: "downloaderror",
        manifestURL:  aNewApp.manifestURL
      });
    });
    AppDownloadManager.remove(aNewApp.manifestURL);
  },

  doUninstall: function(aData, aMm) {
    this.uninstall(aData.manifestURL,
      function onsuccess() {
        aMm.sendAsyncMessage("Webapps:Uninstall:Return:OK", aData);
      },
      function onfailure() {
        // Fall-through, fails to uninstall the desired app because:
        //   - we cannot find the app to be uninstalled.
        //   - the app to be uninstalled is not removable.
        aMm.sendAsyncMessage("Webapps:Uninstall:Return:KO", aData);
      }
    );
  },

  uninstall: function(aManifestURL, aOnSuccess, aOnFailure) {
    debug("uninstall " + aManifestURL);

    let app = this.getAppByManifestURL(aManifestURL);
    if (!app) {
      aOnFailure("NO_SUCH_APP");
      return;
    }
    let id = app.id;

    if (!app.removable) {
      debug("Error: cannot uninstall a non-removable app.");
      aOnFailure("NON_REMOVABLE_APP");
      return;
    }

    // Check if we are downloading something for this app, and cancel the
    // download if needed.
    this.cancelDownload(app.manifestURL);

    // Clean up the deprecated manifest cache if needed.
    if (id in this._manifestCache) {
      delete this._manifestCache[id];
    }

    // Clear private data first.
    this._clearPrivateData(app.localId, false);

    // Then notify observers.
    // We have to clone the app object as nsIDOMApplication objects are
    // stringified as an empty object. (see bug 830376)
    let appClone = AppsUtils.cloneAppObject(app);
    Services.obs.notifyObservers(null, "webapps-uninstall", JSON.stringify(appClone));

    if (supportSystemMessages()) {
      this._readManifests([{ id: id }]).then((aResult) => {
        this._unregisterActivities(aResult[0].manifest, app);
      });
    }

    let dir = this._getAppDir(id);
    try {
      dir.remove(true);
    } catch (e) {}

    delete this.webapps[id];

    this._saveApps().then(() => {
      this.broadcastMessage("Webapps:Uninstall:Broadcast:Return:OK", appClone);
      // Catch exception on callback call to ensure notifying observers after
      try {
        if (aOnSuccess) {
          aOnSuccess();
        }
      } catch(ex) {
        Cu.reportError("DOMApplicationRegistry: Exception on app uninstall: " +
                       ex + "\n" + ex.stack);
      }
      this.broadcastMessage("Webapps:RemoveApp", { id: id });
    });
  },

  getSelf: function(aData, aMm) {
    aData.apps = [];

    if (aData.appId == Ci.nsIScriptSecurityManager.NO_APP_ID ||
        aData.appId == Ci.nsIScriptSecurityManager.UNKNOWN_APP_ID) {
      aMm.sendAsyncMessage("Webapps:GetSelf:Return:OK", aData);
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
      aMm.sendAsyncMessage("Webapps:GetSelf:Return:OK", aData);
      return;
    }

    this._readManifests(tmp).then((aResult) => {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      aMm.sendAsyncMessage("Webapps:GetSelf:Return:OK", aData);
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
      aMm.sendAsyncMessage("Webapps:CheckInstalled:Return:OK", aData);
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
      aMm.sendAsyncMessage("Webapps:GetInstalled:Return:OK", aData);
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
      aMm.sendAsyncMessage("Webapps:GetNotInstalled:Return:OK", aData);
    });
  },

  doGetAll: function(aData, aMm) {
    this.getAll(function (apps) {
      aData.apps = apps;
      aMm.sendAsyncMessage("Webapps:GetAll:Return:OK", aData);
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

  getManifestFor: function(aManifestURL) {
    let id = this._appIdForManifestURL(aManifestURL);
    let app = this.webapps[id];
    if (!id || (app.installState == "pending" && !app.retryingDownload)) {
      return Promise.resolve(null);
    }

    return this._readManifests([{ id: id }]).then((aResult) => {
      return aResult[0].manifest;
    });
  },

  getAppByManifestURL: function(aManifestURL) {
    return AppsUtils.getAppByManifestURL(this.webapps, aManifestURL);
  },

  getCSPByLocalId: function(aLocalId) {
    debug("getCSPByLocalId:" + aLocalId);
    return AppsUtils.getCSPByLocalId(this.webapps, aLocalId);
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

  registerBrowserElementParentForApp: function(bep, appId) {
    let mm = bep._mm;

    // Make a listener function that holds on to this appId.
    let listener = this.receiveAppMessage.bind(this, appId);

    this.frameMessages.forEach(function(msgName) {
      mm.addMessageListener(msgName, listener);
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
      manifestURL: app.manifestURL
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
        manifestURL: app.manifestURL
      });
      DOMApplicationRegistry.broadcastMessage("Webapps:FireEvent", {
        eventType: ["downloadsuccess", "downloadapplied"],
        manifestURL: app.manifestURL
      });
    }

    let setError = function appObs_setError(aError) {
      debug("Offlinecache setError to " + aError);
      app.downloading = false;
      DOMApplicationRegistry.broadcastMessage("Webapps:UpdateState", {
        app: app,
        manifestURL: app.manifestURL
      });
      DOMApplicationRegistry.broadcastMessage("Webapps:FireEvent", {
        error: aError,
        eventType: "downloaderror",
        manifestURL: app.manifestURL
      });
      mustSave = true;
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
