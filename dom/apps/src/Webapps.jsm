/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

let EXPORTED_SYMBOLS = ["DOMApplicationRegistry", "DOMApplicationManifest"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import('resource://gre/modules/ActivitiesService.jsm');
Cu.import("resource://gre/modules/AppsUtils.jsm");

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

XPCOMUtils.defineLazyGetter(this, "msgmgr", function() {
  return Cc["@mozilla.org/system-message-internal;1"]
         .getService(Ci.nsISystemMessagesInternal);
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

let DOMApplicationRegistry = {
  appsFile: null,
  webapps: { },
  children: [ ],
  allAppsLaunchable: false,

  init: function() {
    this.messages = ["Webapps:Install", "Webapps:Uninstall",
                     "Webapps:GetSelf",
                     "Webapps:GetInstalled", "Webapps:GetNotInstalled",
                     "Webapps:Launch", "Webapps:GetAll",
                     "Webapps:InstallPackage", "Webapps:GetBasePath",
                     "Webapps:GetList", "Webapps:RegisterForMessages",
                     "Webapps:UnregisterForMessages"];

    this.messages.forEach((function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }).bind(this));

    cpmm.addMessageListener("Activities:Register:OK", this);

    Services.obs.addObserver(this, "xpcom-shutdown", false);

    this.appsFile = FileUtils.getFile(DIRECTORY_NAME,
                                      ["webapps", "webapps.json"], true);

    this.loadAndUpdateApps();
  },

  // loads the current registry, that could be empty on first run.
  // aNext() is called after we load the current webapps list.
  loadCurrentRegistry: function loadCurrentRegistry(aNext) {
    let file = FileUtils.getFile(DIRECTORY_NAME, ["webapps", "webapps.json"], false);
    if (file && file.exists) {
      this._loadJSONAsync(file, (function loadRegistry(aData) {
        if (aData) {
          this.webapps = aData;
          let appDir = FileUtils.getDir(DIRECTORY_NAME, ["webapps"], false);
          for (let id in this.webapps) {
            // Make sure we have a localId
            if (this.webapps[id].localId === undefined) {
              this.webapps[id].localId = this._nextLocalId();
            }

            if (this.webapps[id].basePath === undefined) {
              this.webapps[id].basePath = appDir.path;
            }

            // Default to removable apps.
            if (this.webapps[id].removable === undefined) {
              this.webapps[id].removable = true;
            }

            // Default to a non privileged status.
            if (this.webapps[id].appStatus === undefined) {
              this.webapps[id].appStatus = Ci.nsIPrincipal.APP_STATUS_INSTALLED;
            }
          };
        }
        aNext();
      }).bind(this));
    } else {
      aNext();
    }
  },

  // We are done with loading and initializing. Notify and
  // save a copy of the registry.
  onInitDone: function onInitDone() {
    Services.obs.notifyObservers(this, "webapps-registry-ready", null);
    this._saveApps();
  },

  // registers all the activities and system messages
  registerAppsHandlers: function registerAppsHandlers() {
#ifdef MOZ_SYS_MSG
    let ids = [];
    for (let id in this.webapps) {
      ids.push({ id: id });
    }
    this._processManifestForIds(ids);
#else
    // Nothing else to do but notifying we're ready.
    this.onInitDone();
#endif
  },

  // Implements the core of bug 787439
  // 1. load the apps from the current registry.
  // 2. if at first run, go through these steps:
  //   a. load the core apps registry.
  //   b. uninstall any core app from the current registry but not in the
  //      new core apps registry.
  //   c. for all apps in the new core registry, install them if they are not
  //      yet in the current registry, and run installPermissions()
  loadAndUpdateApps: function loadAndUpdateApps() {
    let runUpdate = Services.prefs.getBoolPref("dom.mozApps.runUpdate");
    Services.prefs.setBoolPref("dom.mozApps.runUpdate", false);

    // 1.
    this.loadCurrentRegistry((function() {
#ifdef MOZ_WIDGET_GONK
    // if first run, merge the system apps.
    if (runUpdate) {
      let file;
      try {
        file = FileUtils.getFile("coreAppsDir", ["webapps", "webapps.json"], false);
      } catch(e) { }

      if (file && file.exists) {
        // 2.a
        this._loadJSONAsync(file, (function loadCoreRegistry(aData) {
          if (!aData) {
            this.registerAppsHandlers();
            return;
          }

          // 2.b : core apps are not removable.
          for (let id in this.webapps) {
            if (id in aData || this.webapps[id].removable)
              continue;
            let localId = this.webapps[id].localId;
            delete this.webapps[id];
            // XXXX once bug 758269 is ready, revoke perms for this app
            // removePermissions(localId);
          }

          let appDir = FileUtils.getDir("coreAppsDir", ["webapps"], false);
          // 2.c
          for (let id in aData) {
            // Core apps have ids matching their domain name (eg: dialer.gaiamobile.org)
            // Use that property to check if they are new or not.
            if (!(id in this.webapps)) {
              this.webapps[id] = aData[id];
              this.webapps[id].basePath = appDir.path;

              // Create a new localId.
              this.webapps[id].localId = this._nextLocalId();

              // Core apps are not removable.
              if (this.webapps[id].removable === undefined) {
                this.webapps[id].removable = false;
              }
            }
            // XXXX once bug 758269 is ready, revoke perms for this app
            // let localId = this.webapps[id].localId;
            // installPermissions(localId);
          }
          this.registerAppsHandlers();
        }).bind(this));
      } else {
        this.registerAppsHandlers();
      }
    } else {
      this.registerAppsHandlers();
    }
#else
    this.registerAppsHandlers();
#endif
    }).bind(this));
  },

#ifdef MOZ_SYS_MSG

  // aEntryPoint is either the entry_point name or the null, in which case we
  // use the root of the manifest.
  _registerSystemMessagesForEntryPoint: function(aManifest, aApp, aEntryPoint) {
    let root = aManifest;
    if (aEntryPoint && aManifest.entry_points[aEntryPoint]) {
      root = aManifest.entry_points[aEntryPoint];
    }

    if (!root.messages || !Array.isArray(root.messages) ||
        root.messages.length == 0) {
      return;
    }

    let manifest = new DOMApplicationManifest(aManifest, aApp.origin);
    let launchPath = Services.io.newURI(manifest.fullLaunchPath(aEntryPoint), null, null);
    let manifestURL = Services.io.newURI(aApp.manifestURL, null, null);
    root.messages.forEach(function registerPages(aMessage) {
      let href = launchPath;
      let messageName;
      if (typeof(aMessage) === "object" && Object.keys(aMessage).length === 1) {
        messageName = Object.keys(aMessage)[0];
        href = Services.io.newURI(manifest.resolveFromOrigin(aMessage[messageName]), null, null);
      } else {
        messageName = aMessage;
      }
      msgmgr.registerPage(messageName, href, manifestURL);
    });
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

  // aEntryPoint is either the entry_point name or the null, in which case we
  // use the root of the manifest.
  _registerActivitiesForEntryPoint: function(aManifest, aApp, aEntryPoint) {
    let root = aManifest;
    if (aEntryPoint && aManifest.entry_points[aEntryPoint]) {
      root = aManifest.entry_points[aEntryPoint];
    }

    if (!root.activities) {
      return;
    }

    let manifest = new DOMApplicationManifest(aManifest, aApp.origin);
    for (let activity in root.activities) {
      let description = root.activities[activity];
      if (!description.href) {
        description.href = manifest.launch_path;
      }
      description.href = manifest.resolveFromOrigin(description.href);
      let json = {
        "manifest": aApp.manifestURL,
        "name": activity,
        "title": manifest.name,
        "icon": manifest.iconURLForSize(128),
        "description": description
      }
      this.activitiesToRegister++;
      cpmm.sendAsyncMessage("Activities:Register", json);

      let launchPath =
        Services.io.newURI(manifest.resolveFromOrigin(description.href), null, null);
      let manifestURL = Services.io.newURI(aApp.manifestURL, null, null);
      msgmgr.registerPage("activity", launchPath, manifestURL);
    }
  },

  _registerActivities: function(aManifest, aApp) {
    this._registerActivitiesForEntryPoint(aManifest, aApp, null);

    if (!aManifest.entry_points) {
      return;
    }

    for (let entryPoint in aManifest.entry_points) {
      this._registerActivitiesForEntryPoint(aManifest, aApp, entryPoint);
    }
  },

  _unregisterActivitiesForEntryPoint: function(aManifest, aApp, aEntryPoint) {
    let root = aManifest;
    if (aEntryPoint && aManifest.entry_points[aEntryPoint]) {
      root = aManifest.entry_points[aEntryPoint];
    }

    if (!root.activities) {
      return;
    }

    for (let activity in root.activities) {
      let description = root.activities[activity];
      let json = {
        "manifest": aApp.manifestURL,
        "name": activity
      }
      cpmm.sendAsyncMessage("Activities:Unregister", json);
    }
  },

  _unregisterActivities: function(aManifest, aApp) {
    this._unregisterActivitiesForEntryPoint(aManifest, aApp, null);

    if (!aManifest.entry_points) {
      return;
    }

    for (let entryPoint in aManifest.entry_points) {
      this._unregisterActivitiesForEntryPoint(aManifest, aApp, entryPoint);
    }
  },

  _processManifestForIds: function(aIds) {
    this.activitiesToRegister = 0;
    this.activitiesRegistered = 0;
    this.allActivitiesSent = false;
    this._readManifests(aIds, (function registerManifests(aResults) {
      aResults.forEach(function registerManifest(aResult) {
        let app = this.webapps[aResult.id];
        let manifest = aResult.manifest;
        app.name = manifest.name;
        this._registerSystemMessages(manifest, app);
        this._registerActivities(manifest, app);
      }, this);
      this.allActivitiesSent = true;
    }).bind(this));
  },
#endif

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      this.messages.forEach((function(msgName) {
        ppmm.removeMessageListener(msgName, this);
      }).bind(this));
      Services.obs.removeObserver(this, "xpcom-shutdown");
      ppmm = null;
    }
  },

  _loadJSONAsync: function(aFile, aCallback) {
    try {
      let channel = NetUtil.newChannel(aFile);
      channel.contentType = "application/json";
      NetUtil.asyncFetch(channel, function(aStream, aResult) {
        if (!Components.isSuccessCode(aResult)) {
          Cu.reportError("DOMApplicationRegistry: Could not read from json file "
                         + aFile.path);
          if (aCallback)
            aCallback(null);
          return;
        }

        // Read json file into a string
        let data = null;
        try {
          // Obtain a converter to read from a UTF-8 encoded input stream.
          let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                          .createInstance(Ci.nsIScriptableUnicodeConverter);
          converter.charset = "UTF-8";

          data = JSON.parse(converter.ConvertToUnicode(NetUtil.readInputStreamToString(aStream,
                                                            aStream.available()) || ""));
          aStream.close();
          if (aCallback)
            aCallback(data);
        } catch (ex) {
          Cu.reportError("DOMApplicationRegistry: Could not parse JSON: " +
                         aFile.path + " " + ex);
          if (aCallback)
            aCallback(null);
        }
      });
    } catch (ex) {
      Cu.reportError("DOMApplicationRegistry: Could not read from " +
                     aFile.path + " : " + ex);
      if (aCallback)
        aCallback(null);
    }
  },

  addMessageListener: function(aMsgNames, aMm) {
    aMsgNames.forEach(function (aMsgName) {
      if (!(aMsgName in this.children)) {
        this.children[aMsgName] = [];
      }
      this.children[aMsgName].push(aMm);
    }, this);
  },

  removeMessageListener: function(aMsgNames, aMm) {
    aMsgNames.forEach(function (aMsgName) {
      if (!(aMsgName in this.children)) {
        return;
      }

      let index;
      if ((index = this.children[aMsgName].indexOf(aMm)) != -1) {
        this.children[aMsgName].splice(index, 1);
      }
    }, this);
  },

  receiveMessage: function(aMessage) {
    // nsIPrefBranch throws if pref does not exist, faster to simply write
    // the pref instead of first checking if it is false.
    Services.prefs.setBoolPref("dom.mozApps.used", true);

    let msg = aMessage.json;
    let mm = aMessage.target;
    msg.mm = mm;

    switch (aMessage.name) {
      case "Webapps:Install":
        // always ask for UI to install
        Services.obs.notifyObservers(mm, "webapps-ask-install", JSON.stringify(msg));
        break;
      case "Webapps:GetSelf":
        this.getSelf(msg, mm);
        break;
      case "Webapps:Uninstall":
        Services.obs.notifyObservers(mm, "webapps-uninstall", JSON.stringify(msg));
        this.uninstall(msg);
        break;
      case "Webapps:Launch":
        Services.obs.notifyObservers(mm, "webapps-launch", JSON.stringify(msg));
        break;
      case "Webapps:GetInstalled":
        this.getInstalled(msg, mm);
        break;
      case "Webapps:GetNotInstalled":
        this.getNotInstalled(msg, mm);
        break;
      case "Webapps:GetAll":
        if (msg.hasPrivileges)
          this.getAll(msg, mm);
        else
          mm.sendAsyncMessage("Webapps:GetAll:Return:KO", msg);
        break;
      case "Webapps:InstallPackage":
        this.installPackage(msg, mm);
        break;
      case "Webapps:GetBasePath":
        return this.webapps[msg.id].basePath;
        break;
      case "Webapps:RegisterForMessages":
        this.addMessageListener(msg, mm);
        break;
      case "Webapps:UnregisterForMessages":
        this.removeMessageListener(msg, mm);
        break;
      case "Webapps:GetList":
        this.addMessageListener(["Webapps:AddApp", "Webapps:RemoveApp"], mm);
        return this.webapps;
      case "Activities:Register:OK":
        this.activitiesRegistered++;
        if (this.allActivitiesSent &&
            this.activitiesRegistered === this.activitiesToRegister) {
          this.onInitDone();
        }
        break;
    }
  },

  // Some messages can be listened by several content processes:
  // Webapps:AddApp
  // Webapps:RemoveApp
  // Webapps:Install:Return:OK
  // Webapps:Uninstall:Return:OK
  // Webapps:OfflineCache
  broadcastMessage: function broadcastMessage(aMsgName, aContent) {
    if (!(aMsgName in this.children)) {
      return;
    }

    this.children[aMsgName].forEach(function _doBroadcast(aMsgMgr) {
      aMsgMgr.sendAsyncMessage(aMsgName, aContent);
    });
  },

  _getAppDir: function(aId) {
    return FileUtils.getDir(DIRECTORY_NAME, ["webapps", aId], true, true);
  },

  _writeFile: function ss_writeFile(aFile, aData, aCallbak) {
    // Initialize the file output stream.
    let ostream = FileUtils.openSafeFileOutputStream(aFile);

    // Obtain a converter to convert our data to a UTF-8 encoded input stream.
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    // Asynchronously copy the data to the file.
    let istream = converter.convertToInputStream(aData);
    NetUtil.asyncCopy(istream, ostream, function(rc) {
      if (aCallbak)
        aCallbak();
    });
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

  confirmInstall: function(aData, aFromSync, aProfileDir, aOfflineCacheObserver) {
    let app = aData.app;
    app.removable = true;
    let id = app.syncId || this._appId(app.origin);
    let localId = this.getAppLocalIdByManifestURL(app.manifestURL);

    // Installing an application again is considered as an update.
    if (id) {
      let dir = this._getAppDir(id);
      try {
        dir.remove(true);
      } catch(e) {
      }
    } else {
      id = this.makeAppId();
      localId = this._nextLocalId();
    }

    if (app.packageId) {
      // Override the origin with the correct id.
      app.origin = "app://" + id;
    }

    let appObject = AppsUtils.cloneAppObject(app);
    appObject.appStatus = app.appStatus || Ci.nsIPrincipal.APP_STATUS_INSTALLED;
    appObject.installTime = app.installTime = Date.now();
    let appNote = JSON.stringify(appObject);
    appNote.id = id;

    appObject.localId = localId;
    appObject.basePath = FileUtils.getDir(DIRECTORY_NAME, ["webapps"], true, true).path;

    let dir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", id], true, true);
    let manFile = dir.clone();
    manFile.append("manifest.webapp");
    this._writeFile(manFile, JSON.stringify(app.manifest), function() {
      // If this a packaged app, move the zip file from the temp directory,
      // and delete the temp directory.
      if (app.packageId) {
        let appFile = FileUtils.getFile("TmpD", ["webapps", app.packageId, "application.zip"],
                                        true, true);
        appFile.moveTo(dir, "application.zip");
        let tmpDir = FileUtils.getDir("TmpD", ["webapps", app.packageId],
                                        true, true);
        try {
          tmpDir.remove(true);
        } catch(e) {
        }
      }
    });
    this.webapps[id] = appObject;

    appObject.status = "installed";
    appObject.name = app.manifest.name;

    let manifest = new DOMApplicationManifest(app.manifest, app.origin);

    if (!aFromSync)
      this._saveApps((function() {
        this.broadcastMessage("Webapps:Install:Return:OK", aData);
        Services.obs.notifyObservers(this, "webapps-sync-install", appNote);
        this.broadcastMessage("Webapps:AddApp", { id: id, app: appObject });
      }).bind(this));

#ifdef MOZ_SYS_MSG
    this._registerSystemMessages(app.manifest, app);
    this._registerActivities(app.manifest, app);
#endif

    // if the manifest has an appcache_path property, use it to populate the appcache
    if (manifest.appcache_path) {
      let appcacheURI = Services.io.newURI(manifest.fullAppcachePath(), null, null);
      let updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"]
                            .getService(Ci.nsIOfflineCacheUpdateService);
      let docURI = Services.io.newURI(manifest.fullLaunchPath(), null, null);
      let cacheUpdate = aProfileDir ? updateService.scheduleCustomProfileUpdate(appcacheURI, docURI, aProfileDir)
                                    : updateService.scheduleUpdate(appcacheURI, docURI, null);
      cacheUpdate.addObserver(new AppcacheObserver(appObject), false);
      if (aOfflineCacheObserver) {
        cacheUpdate.addObserver(aOfflineCacheObserver, false);
      }
    }
  },

  _nextLocalId: function() {
    let id = Services.prefs.getIntPref("dom.mozApps.maxLocalId") + 1;
    Services.prefs.setIntPref("dom.mozApps.maxLocalId", id);
    return id;
  },

  _appId: function(aURI) {
    for (let id in this.webapps) {
      if (this.webapps[id].origin == aURI)
        return id;
    }
    return null;
  },

  makeAppId: function() {
    let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
    return uuidGenerator.generateUUID().toString();
  },

  _saveApps: function(aCallback) {
    this._writeFile(this.appsFile, JSON.stringify(this.webapps), function() {
      if (aCallback)
        aCallback();
    });
  },

  /**
    * Asynchronously reads a list of manifests
    */
  _readManifests: function(aData, aFinalCallback, aIndex) {
    if (!aData.length) {
      aFinalCallback(aData);
      return;
    }

    let index = aIndex || 0;
    let id = aData[index].id;

    // the manifest file used to be named manifest.json, so fallback on this.
    let baseDir = (this.webapps[id].removable ? DIRECTORY_NAME : "coreAppsDir");
    let file = FileUtils.getFile(baseDir, ["webapps", id, "manifest.webapp"], true);
    if (!file.exists()) {
      file = FileUtils.getFile(baseDir, ["webapps", id, "manifest.json"], true);
    }

    this._loadJSONAsync(file, (function(aJSON) {
      aData[index].manifest = aJSON;
      if (index == aData.length - 1)
        aFinalCallback(aData);
      else
        this._readManifests(aData, aFinalCallback, index + 1);
    }).bind(this));
  },

  installPackage: function(aData, aMm) {
    // Here are the steps when installing a package:
    // - create a temp directory where to store the app.
    // - download the zip in this directory.
    // - extract the manifest from the zip and check it.
    // - ask confirmation to the user.
    // - add the new app to the registry.
    // If we fail at any step, we backout the previous ones and return an error.

    let id;
    let manifestURL = "jar:" + aData.url + "!manifest.webapp";
    // Check if we reinstall a known application.
    for (let appId in this.webapps) {
      if (this.webapps[appId].manifestURL == manifestURL) {
        id = appId;
      }
    }

    // New application.
    if (!id) {
      id = this.makeAppId();
    }

    let dir = FileUtils.getDir("TmpD", ["webapps", id], true, true);

    // Removes the directory we created, and sends an error to the DOM side.
    function cleanup(aError) {
      try {
        dir.remove(true);
      } catch (e) { }
      aMm.sendAsyncMessage("Webapps:Install:Return:KO",
                           { oid: aData.oid,
                             requestID: aData.requestID,
                             error: aError });
    }

    function getInferedStatus() {
      // XXX Update once we have digital signatures (bug 772365)
      return Ci.nsIPrincipal.APP_STATUS_INSTALLED;
    }

    function getAppManifestStatus(aManifest) {
      let type = aManifest.type || "web";
      let manifestStatus = Ci.nsIPrincipal.APP_STATUS_INSTALLED;

      switch(type) {
        case "web":
          manifestStatus = Ci.nsIPrincipal.APP_STATUS_INSTALLED;
          break;
        case "privileged":
          manifestStatus = Ci.nsIPrincipal.APP_STATUS_PRIVILEGED;
          break
        case "certified":
          manifestStatus = Ci.nsIPrincipal.APP_STATUS_CERTIFIED;
          break;
      }

      return manifestStatus;
    }

    function getAppStatus(aManifest) {
      let manifestStatus = getAppManifestStatus(aManifest);
      let inferedStatus = getInferedStatus();

      return (Services.prefs.getBoolPref("dom.mozApps.dev_mode") ? manifestStatus
                                                                : inferedStatus);
    }
    // Returns true if the privilege level from the manifest
    // is lower or equal to the one we infered for the app.
    function checkAppStatus(aManifest) {
      if (Services.prefs.getBoolPref("dom.mozApps.dev_mode")) {
        return true;
      }

      return (getAppManifestStatus(aManifest) <= getInferedStatus());
    }

    NetUtil.asyncFetch(aData.url, function(aInput, aResult, aRequest) {
      if (!Components.isSuccessCode(aResult)) {
        // We failed to fetch the zip.
        cleanup("NETWORK_ERROR");
        return;
      }
      // Copy the zip on disk.
      let zipFile = FileUtils.getFile("TmpD",
                                      ["webapps", id, "application.zip"], true);
      let ostream = FileUtils.openSafeFileOutputStream(zipFile);
      NetUtil.asyncCopy(aInput, ostream, function (aResult) {
        if (!Components.isSuccessCode(aResult)) {
          // We failed to save the zip.
          cleanup("DOWNLOAD_ERROR");
          return;
        }
        // Build a data structure to call the webapps confirmation dialog :
        // - load the manifest from the zip
        // - set data.app.(origin, install_origin, manifestURL, manifest, receipts, categories)
        // - call notifyObservers(this, "webapps-ask-install", JSON.stringify(msg));
        let msg = {
          from: aData.from,
          oid: aData.oid,
          requestID: aData.requestID,
          app: {
            packageId: id,
            installOrigin: aData.installOrigin,
            origin: "app://" + id,
            manifestURL: manifestURL,
            receipts: aData.receipts,
            categories: aData.categories
          }
        }
        let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                        .createInstance(Ci.nsIZipReader);
        try {
          zipReader.open(zipFile);
          if (!zipReader.hasEntry("manifest.webapp")) {
            throw "No manifest.webapp found.";
          }

          let istream = zipReader.getInputStream("manifest.webapp");
          msg.app.manifest = JSON.parse(NetUtil.readInputStreamToString(istream,
                                        istream.available()) || "");
          if (!AppsUtils.checkManifest(msg.app.manifest, aData.installOrigin)) {
            throw "INVALID_MANIFEST";
          }

          if (!checkAppStatus(msg.app.manifest)) {
            throw "INVALID_SECURITY_LEVEL";
          }

          msg.app.appStatus = getAppStatus(msg.app.manifest);
          Services.obs.notifyObservers(this, "webapps-ask-install",
                                             JSON.stringify(msg));
        } catch (e) {
          // XXX we may need new error messages.
          cleanup(e);
        } finally {
          zipReader.close();
        }
      });
    });
  },

  uninstall: function(aData) {
    let found = false;
    for (let id in this.webapps) {
      let app = this.webapps[id];
      if (app.origin != aData.origin) {
        continue;
      }

      if (!this.webapps[id].removable)
        return;

      found = true;
      let appNote = JSON.stringify(AppsUtils.cloneAppObject(app));
      appNote.id = id;

      this._readManifests([{ id: id }], (function unregisterManifest(aResult) {
#ifdef MOZ_SYS_MSG
        this._unregisterActivities(aResult[0].manifest, app);
#endif
      }).bind(this));

      let dir = this._getAppDir(id);
      try {
        dir.remove(true);
      } catch (e) {}

      delete this.webapps[id];

      this._saveApps((function() {
        this.broadcastMessage("Webapps:Uninstall:Return:OK", aData);
        Services.obs.notifyObservers(this, "webapps-sync-uninstall", appNote);
        this.broadcastMessage("Webapps:RemoveApp", { id: id });
      }).bind(this));
    }

    if (!found) {
      aData.mm.broadcastMessage("Webapps:Uninstall:Return:KO", aData);
    }
  },

  getSelf: function(aData, aMm) {
    aData.apps = [];
    let tmp = [];
    let id = this._appId(aData.origin);

    if (id && this._isLaunchable(this.webapps[id].origin)) {
      let app = AppsUtils.cloneAppObject(this.webapps[id]);
      aData.apps.push(app);
      tmp.push({ id: id });
    }

    this._readManifests(tmp, (function(aResult) {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      aMm.sendAsyncMessage("Webapps:GetSelf:Return:OK", aData);
    }).bind(this));
  },

  getInstalled: function(aData, aMm) {
    aData.apps = [];
    let tmp = [];

    for (let id in this.webapps) {
      if (this.webapps[id].installOrigin == aData.origin &&
          this._isLaunchable(this.webapps[id].origin)) {
        aData.apps.push(AppsUtils.cloneAppObject(this.webapps[id]));
        tmp.push({ id: id });
      }
    }

    this._readManifests(tmp, (function(aResult) {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      aMm.sendAsyncMessage("Webapps:GetInstalled:Return:OK", aData);
    }).bind(this));
  },

  getNotInstalled: function(aData, aMm) {
    aData.apps = [];
    let tmp = [];

    for (let id in this.webapps) {
      if (!this._isLaunchable(this.webapps[id].origin)) {
        aData.apps.push(AppsUtils.cloneAppObject(this.webapps[id]));
        tmp.push({ id: id });
      }
    }

    this._readManifests(tmp, (function(aResult) {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      aMm.sendAsyncMessage("Webapps:GetNotInstalled:Return:OK", aData);
    }).bind(this));
  },

  getAll: function(aData, aMm) {
    aData.apps = [];
    let tmp = [];

    for (let id in this.webapps) {
      let app = AppsUtils.cloneAppObject(this.webapps[id]);
      if (!this._isLaunchable(app.origin))
        continue;

      aData.apps.push(app);
      tmp.push({ id: id });
    }

    this._readManifests(tmp, (function(aResult) {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      aMm.sendAsyncMessage("Webapps:GetAll:Return:OK", aData);
    }).bind(this));
  },

  getManifestFor: function(aOrigin, aCallback) {
    if (!aCallback)
      return;

    let id = this._appId(aOrigin);
    if (!id) {
      aCallback(null);
      return;
    }

    this._readManifests([{ id: id }], function(aResult) {
      aCallback(aResult[0].manifest);
    });
  },

  /** Added to support AITC and classic sync */
  itemExists: function(aId) {
    return !!this.webapps[aId];
  },

  getAppById: function(aId) {
    if (!this.webapps[aId])
      return null;

    let app = AppsUtils.cloneAppObject(this.webapps[aId]);
    return app;
  },

  getAppByManifestURL: function(aManifestURL) {
    return AppsUtils.getAppByManifestURL(this.webapps, aManifestURL);
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

  getAppFromObserverMessage: function(aMessage) {
    return AppsUtils.getAppFromObserverMessage(this.webapps, aMessage);
  },

  getAllWithoutManifests: function(aCallback) {
    let result = {};
    for (let id in this.webapps) {
      let app = AppsUtils.cloneAppObject(this.webapps[id]);
      result[id] = app;
    }
    aCallback(result);
  },

  updateApps: function(aRecords, aCallback) {
    for (let i = 0; i < aRecords.length; i++) {
      let record = aRecords[i];
      if (record.hidden) {
        if (!this.webapps[record.id] || !this.webapps[record.id].removable)
          continue;
        let origin = this.webapps[record.id].origin;
        delete this.webapps[record.id];
        let dir = this._getAppDir(record.id);
        try {
          dir.remove(true);
        } catch (e) {
        }
        this.broadcastMessage("Webapps:Uninstall:Return:OK", { origin: origin });
      } else {
        if (this.webapps[record.id]) {
          this.webapps[record.id] = record.value;
          delete this.webapps[record.id].manifest;
        } else {
          let data = { app: record.value };
          this.confirmInstall(data, true);
          this.broadcastMessage("Webapps:Install:Return:OK", data);
        }
      }
    }
    this._saveApps(aCallback);
  },

  getAllIDs: function() {
    let apps = {};
    for (let id in this.webapps) {
      // only sync http and https apps
      if (this.webapps[id].origin.indexOf("http") == 0)
        apps[id] = true;
    }
    return apps;
  },

  wipe: function(aCallback) {
    let ids = this.getAllIDs();
    for (let id in ids) {
      if (!this.webapps[id].removable) {
        continue;
      }

      delete this.webapps[id];
      let dir = this._getAppDir(id);
      try {
        dir.remove(true);
      } catch (e) {
      }
    }
    this._saveApps(aCallback);
  },

  _isLaunchable: function(aOrigin) {
    if (this.allAppsLaunchable)
      return true;

#ifdef XP_WIN
    let uninstallKey = Cc["@mozilla.org/windows-registry-key;1"]
                         .createInstance(Ci.nsIWindowsRegKey);
    try {
      uninstallKey.open(uninstallKey.ROOT_KEY_CURRENT_USER,
                        "SOFTWARE\\Microsoft\\Windows\\" +
                        "CurrentVersion\\Uninstall\\" +
                        aOrigin,
                        uninstallKey.ACCESS_READ);
      uninstallKey.close();
      return true;
    } catch (ex) {
      return false;
    }
#elifdef XP_MACOSX
    let mwaUtils = Cc["@mozilla.org/widget/mac-web-app-utils;1"]
                     .createInstance(Ci.nsIMacWebAppUtils);

    return !!mwaUtils.pathForAppWithIdentifier(aOrigin);
#elifdef XP_UNIX
    let env = Cc["@mozilla.org/process/environment;1"]
                .getService(Ci.nsIEnvironment);
    let xdg_data_home_env = env.get("XDG_DATA_HOME");

    let desktopINI;
    if (xdg_data_home_env != "") {
      desktopINI = Cc["@mozilla.org/file/local;1"]
                     .createInstance(Ci.nsIFile);
      desktopINI.initWithPath(xdg_data_home_env);
    }
    else {
      desktopINI = Services.dirsvc.get("Home", Ci.nsIFile);
      desktopINI.append(".local");
      desktopINI.append("share");
    }
    desktopINI.append("applications");

    let origin = Services.io.newURI(aOrigin, null, null);
    let uniqueName = origin.scheme + ";" +
                     origin.host +
                     (origin.port != -1 ? ";" + origin.port : "");

    desktopINI.append("owa-" + uniqueName + ".desktop");

    return desktopINI.exists();
#else
    return true;
#endif

  }
};

/**
 * Appcache download observer
 */
let AppcacheObserver = function(aApp) {
  this.app = aApp;
};

AppcacheObserver.prototype = {
  // nsIOfflineCacheUpdateObserver implementation
  updateStateChanged: function appObs_Update(aUpdate, aState) {
    let mustSave = false;
    let app = this.app;

    let setStatus = function appObs_setStatus(aStatus) {
      mustSave = (app.status != aStatus);
      app.status = aStatus;
      DOMApplicationRegistry.broadcastMessage("Webapps:OfflineCache",
                                              { manifest: app.manifestURL,
                                                status: aStatus });
    }

    switch (aState) {
      case Ci.nsIOfflineCacheUpdateObserver.STATE_ERROR:
        aUpdate.removeObserver(this);
        setStatus("cache-error");
        break;
      case Ci.nsIOfflineCacheUpdateObserver.STATE_NOUPDATE:
      case Ci.nsIOfflineCacheUpdateObserver.STATE_FINISHED:
        aUpdate.removeObserver(this);
        setStatus("cached");
        break;
      case Ci.nsIOfflineCacheUpdateObserver.STATE_DOWNLOADING:
      case Ci.nsIOfflineCacheUpdateObserver.STATE_ITEMSTARTED:
      case Ci.nsIOfflineCacheUpdateObserver.STATE_ITEMPROGRESS:
        setStatus("downloading")
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

/**
 * Helper object to access manifest information with locale support
 */
let DOMApplicationManifest = function(aManifest, aOrigin) {
  this._origin = Services.io.newURI(aOrigin, null, null);
  this._manifest = aManifest;
  let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry)
                                                          .QueryInterface(Ci.nsIToolkitChromeRegistry);
  let locale = chrome.getSelectedLocale("browser").toLowerCase();
  this._localeRoot = this._manifest;

  if (this._manifest.locales && this._manifest.locales[locale]) {
    this._localeRoot = this._manifest.locales[locale];
  }
  else if (this._manifest.locales) {
    // try with the language part of the locale ("en" for en-GB) only
    let lang = locale.split('-')[0];
    if (lang != locale && this._manifest.locales[lang])
      this._localeRoot = this._manifest.locales[lang];
  }
};

DOMApplicationManifest.prototype = {
  _localeProp: function(aProp) {
    if (this._localeRoot[aProp] != undefined)
      return this._localeRoot[aProp];
    return this._manifest[aProp];
  },

  get name() {
    return this._localeProp("name");
  },

  get description() {
    return this._localeProp("description");
  },

  get version() {
    return this._localeProp("version");
  },

  get launch_path() {
    return this._localeProp("launch_path");
  },

  get developer() {
    return this._localeProp("developer");
  },

  get icons() {
    return this._localeProp("icons");
  },

  get appcache_path() {
    return this._localeProp("appcache_path");
  },

  get orientation() {
    return this._localeProp("orientation");
  },

  iconURLForSize: function(aSize) {
    let icons = this._localeProp("icons");
    if (!icons)
      return null;
    let dist = 100000;
    let icon = null;
    for (let size in icons) {
      let iSize = parseInt(size);
      if (Math.abs(iSize - aSize) < dist) {
        icon = this._origin.resolve(icons[size]);
        dist = Math.abs(iSize - aSize);
      }
    }
    return icon;
  },

  fullLaunchPath: function(aStartPoint) {
    // If no start point is specified, we use the root launch path.
    // In all error cases, we just return null.
    if ((aStartPoint || "") === "") {
      return this._origin.resolve(this._localeProp("launch_path") || "");
    }

    // Search for the l10n entry_points property.
    let entryPoints = this._localeProp("entry_points");
    if (!entryPoints) {
      return null;
    }

    if (entryPoints[aStartPoint]) {
      return this._origin.resolve(entryPoints[aStartPoint].launch_path || "");
    }

    return null;
  },

  resolveFromOrigin: function(aURI) {
    return this._origin.resolve(aURI);
  },

  fullAppcachePath: function() {
    let appcachePath = this._localeProp("appcache_path");
    return this._origin.resolve(appcachePath ? appcachePath : "");
  }
};

DOMApplicationRegistry.init();
