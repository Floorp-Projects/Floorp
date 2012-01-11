/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fabrice Desré <fabrice@mozilla.com>
 *   Mark Finkle <mfinkle@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cu = Components.utils; 
const Cc = Components.classes;
const Ci = Components.interfaces;

let EXPORTED_SYMBOLS = ["DOMApplicationRegistry", "DOMApplicationManifest"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

let DOMApplicationRegistry = {
  appsFile: null,
  webapps: { },

  init: function() {
    this.mm = Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
    let messages = ["Webapps:Install", "Webapps:Uninstall",
                    "Webapps:Enumerate", "Webapps:EnumerateAll", "Webapps:Launch"];

    messages.forEach((function(msgName) {
      this.mm.addMessageListener(msgName, this);
    }).bind(this));

    let appsDir = FileUtils.getDir("ProfD", ["webapps"], true, true);
    this.appsFile = FileUtils.getFile("ProfD", ["webapps", "webapps.json"], true);

    if (!this.appsFile.exists())
      return;

    this._loadJSONAsync(this.appsFile, (function(aData) { this.webapps = aData; }).bind(this));

    try {
      let hosts = Services.prefs.getCharPref("dom.mozApps.whitelist")
      hosts.split(",").forEach(function(aHost) {
        Services.perms.add(Services.io.newURI(aHost, null, null), "webapps-manage",
                           Ci.nsIPermissionManager.ALLOW_ACTION);
      });
    } catch(e) { }
  },

  _loadJSONAsync: function(aFile, aCallback) {
    try {
      let channel = NetUtil.newChannel(aFile);
      channel.contentType = "application/json";
      NetUtil.asyncFetch(channel, function(aStream, aResult) {
        if (!Components.isSuccessCode(aResult)) {
          Cu.reportError("DOMApplicationRegistry: Could not read from json file " + aFile.path);
          if (aCallback)
            aCallback(null);
          return;
        }

        // Read json file into a string
        let data = null;
        try {
          data = JSON.parse(NetUtil.readInputStreamToString(aStream, aStream.available()) || "");
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
      Cu.reportError("DOMApplicationRegistry: Could not read from " + aFile.path + " : " + ex);
      if (aCallback)
        aCallback(null);
    }
  },

  receiveMessage: function(aMessage) {
    let msg = aMessage.json;
    let from = Services.io.newURI(msg.from, null, null);
    let perm = Services.perms.testExactPermission(from, "webapps-manage");

    //only pages with perm set and chrome or about pages can uninstall, enumerate all set oninstall an onuninstall
    let hasPrivileges = perm == Ci.nsIPermissionManager.ALLOW_ACTION || from.schemeIs("chrome") || from.schemeIs("about");

    switch (aMessage.name) {
      case "Webapps:Install":
        // always ask for UI to install
        Services.obs.notifyObservers(this, "webapps-ask-install", JSON.stringify(msg));
        break;
      case "Webapps:Uninstall":
        if (hasPrivileges)
          this.uninstall(msg);
        break;
      case "Webapps:Launch":
        Services.obs.notifyObservers(this, "webapps-launch", JSON.stringify(msg));
        break;
      case "Webapps:Enumerate":
        this.enumerate(msg);
        break;
      case "Webapps:EnumerateAll":
        this.enumerateAll(msg);
        break;
    }
  },

  _writeFile: function ss_writeFile(aFile, aData, aCallbak) {
    // Initialize the file output stream.
    let ostream = FileUtils.openSafeFileOutputStream(aFile);

    // Obtain a converter to convert our data to a UTF-8 encoded input stream.
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].createInstance(Ci.nsIScriptableUnicodeConverter);
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
      receipt: aApp.receipt,
      installTime: aApp.installTime
    };
    return clone;
  },

  denyInstall: function(aData) {
    this.mm.sendAsyncMessage("Webapps:Install:Return:KO", aData);
  },

  confirmInstall: function(aData, aFromSync) {
    let app = aData.app;
    let id = app.syncId || this._appId(app.origin);

    // install an application again is considered as an update
    if (id) {
      let dir = FileUtils.getDir("ProfD", ["webapps", id], true, true);
      try {
        dir.remove(true);
      } catch(e) {
      }
    }
    else {
      let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
      id = uuidGenerator.generateUUID().toString();
    }

    let dir = FileUtils.getDir("ProfD", ["webapps", id], true, true);

    let manFile = dir.clone();
    manFile.append("manifest.json");
    this._writeFile(manFile, JSON.stringify(app.manifest));

    this.webapps[id] = this._cloneAppObject(app);
    delete this.webapps[id].manifest;
    this.webapps[id].installTime = (new Date()).getTime()

    
    if (!aFromSync)
      this._saveApps((function() {
        this.mm.sendAsyncMessage("Webapps:Install:Return:OK", aData);
        Services.obs.notifyObservers(this, "webapps-sync-install", id);
      }).bind(this));
  },

  _appId: function(aURI) {
    for (let id in this.webapps) {
      if (this.webapps[id].origin == aURI)
        return id;
    }
    return null;
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
    let file = FileUtils.getFile("ProfD", ["webapps", id, "manifest.json"], true);
    this._loadJSONAsync(file, (function(aJSON) {
      aData[index].manifest = aJSON;
      if (index == aData.length - 1)
        aFinalCallback(aData);
      else
        this._readManifests(aData, aFinalCallback, index + 1);
    }).bind(this)); 
  },

  uninstall: function(aData) {
    for (let id in this.webapps) {
      let app = this.webapps[id];
      if (app.origin == aData.origin) {
        delete this.webapps[id];
        let dir = FileUtils.getDir("ProfD", ["webapps", id], true, true);
        try {
          dir.remove(true);
        } catch (e) {
        }
        this._saveApps((function() {
          this.mm.sendAsyncMessage("Webapps:Uninstall:Return:OK", aData);
          Services.obs.notifyObservers(this, "webapps-sync-uninstall", id);
        }).bind(this));
      }
    }
  },

  enumerate: function(aData) {
    aData.apps = [];
    let tmp = [];
    let selfId;

    let id = this._appId(aData.origin);
    // if it's an app, add itself to the result
    if (id) {
      let app = this._cloneAppObject(this.webapps[id]);
      aData.apps.push(app);
      tmp.push({ id: id });
      selfId = id;
    }

    // check if it's a store.
    let isStore = false;
    for (id in this.webapps) {
      let app = this.webapps[id];
      if (app.installOrigin == aData.origin) {
        isStore = true;
        break;
      }
    }

    // add all the apps from this store
    if (isStore) {
      for (id in this.webapps) {
        if (id == selfId)
          continue;
        let app = this._cloneAppObject(this.webapps[id]);
        if (app.installOrigin == aData.origin) {
          aData.apps.push(app);
          tmp.push({ id: id });
        }
      }
    }

    this._readManifests(tmp, (function(aResult) {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      this.mm.sendAsyncMessage("Webapps:Enumerate:Return:OK", aData);
    }).bind(this));
  },

  denyEnumerate: function(aData) {
    this.mm.sendAsyncMessage("Webapps:Enumerate:Return:KO", aData);
  },

  enumerateAll: function(aData) {
    aData.apps = [];
    let tmp = [];

    for (id in this.webapps) {
      let app = this._cloneAppObject(this.webapps[id]);
      aData.apps.push(app);
      tmp.push({ id: id });
    }

    this._readManifests(tmp, (function(aResult) {
      for (let i = 0; i < aResult.length; i++)
        aData.apps[i].manifest = aResult[i].manifest;
      this.mm.sendAsyncMessage("Webapps:Enumerate:Return:OK", aData);
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

  /** added to support the sync engine */

  getAppById: function(aId) {
    if (!this.webapps[aId])
      return null;

    let app = this._cloneAppObject(this.webapps[aId]);
    return app;
  },
  
  itemExists: function(aId) {
    return !!this.webapps[aId];
  },

  updateApps: function(aRecords, aCallback) {
    for (let i = 0; i < aRecords.length; i++) {
      let record = aRecords[i];
      if (record.deleted) {
        if (!this.webapps[record.id])
          continue;
        let origin = this.webapps[record.id].origin;
        delete this.webapps[record.id];
        let dir = FileUtils.getDir("ProfD", ["webapps", record.id], true, true);
        try {
          dir.remove(true);
        } catch (e) {
        }
        this.mm.sendAsyncMessage("Webapps:Uninstall:Return:OK", { origin: origin });
      } else {
        if (!!this.webapps[record.id]) {
          this.webapps[record.id] = record.value;
          delete this.webapps[record.id].manifest;
        }
        else {
          let data = { app: record.value };
          this.confirmInstall(data, true);
          this.mm.sendAsyncMessage("Webapps:Install:Return:OK", data);
        }
      }
    }
    this._saveApps(aCallback);
  },

  /*
   * May be removed once sync API change
   */
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
      let dir = FileUtils.getDir("ProfD", ["webapps", id], true, true);
      try {
        dir.remove(true);
      } catch (e) {
      }
    }
    this._saveApps(aCallback);
   }
};

/**
 * Helper object to access manifest information with locale support
 */
DOMApplicationManifest = function(aManifest, aOrigin) {
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
    if (land != locale && this._manifest.locales[lang])
      this._localeRoot = this._manifest.locales[lang];
  }
}

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
  
  fullLaunchPath: function() {
    let launchPath = this._localeProp("launch_path");
    return this._origin.resolve(launchPath ? launchPath : "");
  }
}

DOMApplicationRegistry.init();
