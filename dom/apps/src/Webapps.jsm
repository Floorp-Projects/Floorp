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

const WEBAPP_RUNTIME = Services.appinfo.ID == "webapprt@mozilla.org";

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

XPCOMUtils.defineLazyGetter(this, "ppmm", function() {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"]
         .getService(Ci.nsIFrameMessageManager);
});

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIFrameMessageManager");

XPCOMUtils.defineLazyGetter(this, "msgmgr", function() {
  return Cc["@mozilla.org/system-message-internal;1"]
         .getService(Ci.nsISystemMessagesInternal);
});

#ifdef MOZ_WIDGET_GONK
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
  allAppsLaunchable: false,

  init: function() {
    this.messages = ["Webapps:Install", "Webapps:Uninstall",
                    "Webapps:GetSelf",
                    "Webapps:GetInstalled", "Webapps:GetNotInstalled",
                    "Webapps:Launch", "Webapps:GetAll",
                    "Webapps:InstallPackage", "Webapps:GetBasePath",
                    "WebApps:GetAppByManifestURL", "WebApps:GetAppLocalIdByManifestURL"];

    this.messages.forEach((function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }).bind(this));

    Services.obs.addObserver(this, "xpcom-shutdown", false);

    this.appsFile = FileUtils.getFile(DIRECTORY_NAME,
                                      ["webapps", "webapps.json"], true);

    if (this.appsFile.exists()) {
      this._loadJSONAsync(this.appsFile, (function(aData) {
        this.webapps = aData;
        for (let id in this.webapps) {
#ifdef MOZ_SYS_MSG
          this._processManifestForId(id);
#endif
          if (!this.webapps[id].localId) {
            this.webapps[id].localId = this._nextLocalId();
          }
        };
      }).bind(this));
    }

    try {
      let hosts = Services.prefs.getCharPref("dom.mozApps.whitelist");
      hosts.split(",").forEach(function(aHost) {
        Services.perms.add(Services.io.newURI(aHost, null, null),
                           "webapps-manage",
                           Ci.nsIPermissionManager.ALLOW_ACTION);
      });
    } catch(e) { }
  },

#ifdef MOZ_SYS_MSG
  _registerSystemMessages: function(aManifest, aApp) {
    if (aManifest.messages && Array.isArray(aManifest.messages) &&
        aManifest.messages.length > 0) {
      let manifest = new DOMApplicationManifest(aManifest, aApp.origin);
      let launchPath = Services.io.newURI(manifest.fullLaunchPath(), null, null);
      let manifestURL = Services.io.newURI(aApp.manifestURL, null, null);
      aManifest.messages.forEach(function registerPages(aMessage) {
        msgmgr.registerPage(aMessage, launchPath, manifestURL);
      });
    }
  },

  _registerActivities: function(aManifest, aApp) {
    if (!aManifest.activities) {
      return;
    }

    let manifest = new DOMApplicationManifest(aManifest, aApp.origin);
    for (let activity in aManifest.activities) {
      let description = aManifest.activities[activity];
      let json = {
        "manifest": aApp.manifestURL,
        "name": activity,
        "title": manifest.name,
        "icon": manifest.iconURLForSize(128),
        "description": description
      }
      cpmm.sendAsyncMessage("Activities:Register", json);

      let launchPath =
        Services.io.newURI(manifest.fullLaunchPath(description.href), null, null);
      let manifestURL = Services.io.newURI(aApp.manifestURL, null, null);
      msgmgr.registerPage("activity", launchPath, manifestURL);
    }
  },

  _unregisterActivities: function(aManifest, aApp) {
    if (!aManifest.activities) {
      return;
    }

    for (let activity in aManifest.activities) {
      let description = aManifest.activities[activity];
      let json = {
        "manifest": aApp.manifestURL,
        "name": activity
      }
      cpmm.sendAsyncMessage("Activities:Unregister", json);
    }
  },

  _processManifestForId: function(aId) {
    let app = this.webapps[aId];
    this._readManifests([{ id: aId }], (function registerManifest(aResult) {
      let manifest = aResult[0].manifest;
      this._registerSystemMessages(manifest, app);
      this._registerActivities(manifest, app);
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
          data = JSON.parse(NetUtil.readInputStreamToString(aStream,
                                                            aStream.available()) || "");
          aStream.close();
          if (aCallback)
            aCallback(data);
        } catch (ex) {
          Cu.reportError("DOMApplicationRegistry: Could not parse JSON: " + ex);
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

  receiveMessage: function(aMessage) {
    // nsIPrefBranch throws if pref does not exist, faster to simply write
    // the pref instead of first checking if it is false.
    Services.prefs.setBoolPref("dom.mozApps.used", true);

    let msg = aMessage.json;

    switch (aMessage.name) {
      case "Webapps:Install":
        // always ask for UI to install
        Services.obs.notifyObservers(this, "webapps-ask-install", JSON.stringify(msg));
        break;
      case "Webapps:GetSelf":
        this.getSelf(msg);
        break;
      case "Webapps:Uninstall":
        this.uninstall(msg);
        break;
      case "Webapps:Launch":
        Services.obs.notifyObservers(this, "webapps-launch", JSON.stringify(msg));
        break;
      case "Webapps:GetInstalled":
        this.getInstalled(msg);
        break;
      case "Webapps:GetNotInstalled":
        this.getNotInstalled(msg);
        break;
      case "Webapps:GetAll":
        if (msg.hasPrivileges)
          this.getAll(msg);
        else
          ppmm.sendAsyncMessage("Webapps:GetAll:Return:KO", msg);
        break;
      case "Webapps:InstallPackage":
        this.installPackage(msg);
        break;
      case "Webapps:GetBasePath":
        return FileUtils.getFile(DIRECTORY_NAME, ["webapps"], true).path;
        break;
      case "WebApps:GetAppByManifestURL":
        return this.getAppByManifestURL(msg.url);
        break;
      case "WebApps:GetAppLocalIdByManifestURL":
        return { id: this.getAppLocalIdByManifestURL(msg.url) };
        break;
    }
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

  // clones a app object, without the manifest
  _cloneAppObject: function(aApp) {
    let clone = {
      installOrigin: aApp.installOrigin,
      origin: aApp.origin,
      receipts: aApp.receipts,
      installTime: aApp.installTime,
      manifestURL: aApp.manifestURL,
      progress: aApp.progress || 0.0,
      status: aApp.status || "installed"
    };
    return clone;
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
    ppmm.sendAsyncMessage("Webapps:Install:Return:KO", aData);
  },

  confirmInstall: function(aData, aFromSync, aProfileDir, aOfflineCacheObserver) {
    let app = aData.app;
    let id = app.syncId || this._appId(app.origin);
    let localId = this.getAppLocalIdByManifestURL(app.manifestURL);

    // Installing an application again is considered as an update.
    if (id) {
      let dir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", id], true, true);
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

    let appObject = this._cloneAppObject(app);
    appObject.installTime = app.installTime = Date.now();
    let appNote = JSON.stringify(appObject);
    appNote.id = id;

    appObject.localId = localId;

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
    
    let manifest = new DOMApplicationManifest(app.manifest, app.origin);

    if (!aFromSync)
      this._saveApps((function() {
        ppmm.sendAsyncMessage("Webapps:Install:Return:OK", aData);
        Services.obs.notifyObservers(this, "webapps-sync-install", appNote);
      }).bind(this));

#ifdef MOZ_SYS_MSG
    this._registerSystemMessages(id, app);
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
    let maxLocalId = Ci.nsIScriptSecurityManager.NO_APP_ID;

    for (let id in this.webapps) {
      if (this.webapps[id].localId > maxLocalId) {
        maxLocalId = this.webapps[id].localId;
      }
    }

    return maxLocalId + 1;
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
    let file = FileUtils.getFile(DIRECTORY_NAME, ["webapps", id, "manifest.webapp"], true);
    if (!file.exists()) {
      file = FileUtils.getFile(DIRECTORY_NAME, ["webapps", id, "manifest.json"], true);
    }

    this._loadJSONAsync(file, (function(aJSON) {
      aData[index].manifest = aJSON;
      if (index == aData.length - 1)
        aFinalCallback(aData);
      else
        this._readManifests(aData, aFinalCallback, index + 1);
    }).bind(this));
  },

  installPackage: function(aData) {
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

    /** from https://developer.mozilla.org/en/OpenWebApps/The_Manifest
     * only the name property is mandatory
     */
    function checkManifest(aManifest) {
      if (aManifest.name == undefined)
        return false;

      if (aManifest.installs_allowed_from) {
        return aManifest.installs_allowed_from.some(function(aOrigin) {
          return aOrigin == "*" || aOrigin == aData.installOrigin;
        });
      }
      return true;
    }

    // Removes the directory we created, and sends an error to the DOM side.
    function cleanup(aError) {
      try {
        dir.remove(true);
      } catch (e) { }
      ppmm.sendAsyncMessage("Webapps:Install:Return:KO",
                            { oid: aData.oid,
                              requestID: aData.requestID,
                              error: aError });
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
        // - set data.app.(origin, install_origin, manifestURL, manifest, receipts)
        // - call notifyObservers(this, "webapps-ask-install", JSON.stringify(msg));
        let msg = {
          from: aData.from,
          oid: aData.oid,
          requestId: aData.requestId,
          app: {
            packageId: id,
            installOrigin: aData.installOrigin,
            origin: "app://" + id,
            manifestURL: manifestURL,
            receipts: aData.receipts
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
          if (!checkManifest(msg.app.manifest)) {
            throw "Invalid manifest";
          }

          Services.obs.notifyObservers(this, "webapps-ask-install",
                                             JSON.stringify(msg));
        } catch (e) {
          // XXX we may need new error messages.
          cleanup("INVALID_MANIFEST");
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

      found = true;
      let appNote = JSON.stringify(this._cloneAppObject(app));
      appNote.id = id;

      this._readManifests([{ id: id }], (function unregisterManifest(aResult) {
#ifdef MOZ_SYS_MSG
        this._unregisterActivities(aResult[0].manifest, app);
#endif
      }).bind(this));

      let dir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", id], true, true);
      try {
        dir.remove(true);
      } catch (e) {}

      delete this.webapps[id];

      this._saveApps((function() {
        ppmm.sendAsyncMessage("Webapps:Uninstall:Return:OK", aData);
        Services.obs.notifyObservers(this, "webapps-sync-uninstall", appNote);
      }).bind(this));
    }

    if (!found) {
      ppmm.sendAsyncMessage("Webapps:Uninstall:Return:KO", aData);
    }
  },

  getSelf: function(aData) {
    aData.apps = [];
    let tmp = [];
    let id = this._appId(aData.origin);

    if (id && this._isLaunchable(this.webapps[id].origin)) {
      let app = this._cloneAppObject(this.webapps[id]);
      aData.apps.push(app);
      tmp.push({ id: id });
    }

    this._readManifests(tmp, (function(aResult) {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      ppmm.sendAsyncMessage("Webapps:GetSelf:Return:OK", aData);
    }).bind(this));
  },

  getInstalled: function(aData) {
    aData.apps = [];
    let tmp = [];

    for (let id in this.webapps) {
      if (this.webapps[id].installOrigin == aData.origin &&
          this._isLaunchable(this.webapps[id].origin)) {
        aData.apps.push(this._cloneAppObject(this.webapps[id]));
        tmp.push({ id: id });
      }
    }

    this._readManifests(tmp, (function(aResult) {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      ppmm.sendAsyncMessage("Webapps:GetInstalled:Return:OK", aData);
    }).bind(this));
  },

  getNotInstalled: function(aData) {
    aData.apps = [];
    let tmp = [];

    for (let id in this.webapps) {
      if (this.webapps[id].installOrigin == aData.origin &&
          !this._isLaunchable(this.webapps[id].origin)) {
        aData.apps.push(this._cloneAppObject(this.webapps[id]));
        tmp.push({ id: id });
      }
    }

    this._readManifests(tmp, (function(aResult) {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      ppmm.sendAsyncMessage("Webapps:GetNotInstalled:Return:OK", aData);
    }).bind(this));
  },

  getAll: function(aData) {
    aData.apps = [];
    let tmp = [];

    for (let id in this.webapps) {
      let app = this._cloneAppObject(this.webapps[id]);
      if (!this._isLaunchable(app.origin))
        continue;

      aData.apps.push(app);
      tmp.push({ id: id });
    }

    this._readManifests(tmp, (function(aResult) {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      ppmm.sendAsyncMessage("Webapps:GetAll:Return:OK", aData);
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

    let app = this._cloneAppObject(this.webapps[aId]);
    return app;
  },

  getAppByManifestURL: function(aManifestURL) {
    // This could be O(1) if |webapps| was a dictionary indexed on manifestURL
    // which should be the unique app identifier.
    // It's currently O(n).
    for (let id in this.webapps) {
      let app = this.webapps[id];
      if (app.manifestURL == aManifestURL) {
        return this._cloneAppObject(app);
      }
    }

    return null;
  },

  getAppLocalIdByManifestURL: function(aManifestURL) {
    for (let id in this.webapps) {
      if (this.webapps[id].manifestURL == aManifestURL) {
        return this.webapps[id].localId;
      }
    }

    return Ci.nsIScriptSecurityManager.NO_APP_ID;
  },

  getAllWithoutManifests: function(aCallback) {
    let result = {};
    for (let id in this.webapps) {
      let app = this._cloneAppObject(this.webapps[id]);
      result[id] = app;
    }
    aCallback(result);
  },

  updateApps: function(aRecords, aCallback) {
    for (let i = 0; i < aRecords.length; i++) {
      let record = aRecords[i];
      if (record.hidden) {
        if (!this.webapps[record.id])
          continue;
        let origin = this.webapps[record.id].origin;
        delete this.webapps[record.id];
        let dir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", record.id], true, true);
        try {
          dir.remove(true);
        } catch (e) {
        }
        ppmm.sendAsyncMessage("Webapps:Uninstall:Return:OK", { origin: origin });
      } else {
        if (this.webapps[record.id]) {
          this.webapps[record.id] = record.value;
          delete this.webapps[record.id].manifest;
        } else {
          let data = { app: record.value };
          this.confirmInstall(data, true);
          ppmm.sendAsyncMessage("Webapps:Install:Return:OK", data);
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
      delete this.webapps[id];
      let dir = FileUtils.getDir(DIRECTORY_NAME, ["webapps", id], true, true);
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
      ppmm.sendAsyncMessage("Webapps:OfflineCache", { manifest: app.manifestURL, status: aStatus });
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
    let startPoint = aStartPoint || "";
    let launchPath = this._localeProp("launch_path") || "";
    return this._origin.resolve(launchPath + startPoint);
  },

  fullAppcachePath: function() {
    let appcachePath = this._localeProp("appcache_path");
    return this._origin.resolve(appcachePath ? appcachePath : "");
  }
};

DOMApplicationRegistry.init();
