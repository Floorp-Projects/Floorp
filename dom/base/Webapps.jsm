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

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

let DOMApplicationRegistry = {
  appsDir: null,
  appsFile: null,
  webapps: { },

  init: function() {
    this.mm = Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
    let messages = ["Webapps:Install", "Webapps:Uninstall",
                    "Webapps:Enumerate", "Webapps:Launch"];

    messages.forEach((function(msgName) {
      this.mm.addMessageListener(msgName, this);
    }).bind(this));

    let file =  Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties).get("ProfD", Ci.nsIFile);
    file.append("webapps");
    if (!file.exists() || !file.isDirectory()) {
      file.create(Ci.nsIFile.DIRECTORY_TYPE, 0700);
    }
    this.appsDir = file;
    this.appsFile = file.clone();
    this.appsFile.append("webapps.json");
    if (!this.appsFile.exists())
      return;
    
    try {
      let channel = NetUtil.newChannel(this.appsFile);
      channel.contentType = "application/json";
      let self = this;
      NetUtil.asyncFetch(channel, function(aStream, aResult) {
        if (!Components.isSuccessCode(aResult)) {
          Cu.reportError("DOMApplicationRegistry: Could not read from json file " + this.appsFile.path);
          return;
        }

        // Read json file into a string
        let data = null;
        try {
          self.webapps = JSON.parse(NetUtil.readInputStreamToString(aStream, aStream.available()) || "");
          aStream.close();
        } catch (ex) {
          Cu.reportError("DOMApplicationRegistry: Could not parse JSON: " + ex);
        }
      });
    } catch (ex) {
      Cu.reportError("DOMApplicationRegistry: Could not read from " + aFile.path + " : " + ex);
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
        if (hasPrivileges)
          this.enumerateAll(msg)
        else
          this.enumerate(msg);
        break;
    }
  },

  _writeFile: function ss_writeFile(aFile, aData, aCallbak) {
    // Initialize the file output stream.
    let ostream = Cc["@mozilla.org/network/safe-file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
    ostream.init(aFile, 0x02 | 0x08 | 0x20, 0600, ostream.DEFER_OPEN);

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
  
  confirmInstall: function(aData) {
    let app = aData.app;
    let id = this._appId(app.origin);

    // install an application again is considered as an update
    if (id) {
      let dir = this.appsDir.clone();
      dir.append(id);
      try {
        dir.remove(true);
      } catch(e) {
      }
    }
    else {
      let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
      id = uuidGenerator.generateUUID().toString();
    }

    let dir = this.appsDir.clone();
    dir.append(id);
    dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0700);
    
    let manFile = dir.clone();
    manFile.append("manifest.json");
    this._writeFile(manFile, JSON.stringify(app.manifest));

    this.webapps[id] = this._cloneAppObject(app);
    delete this.webapps[id].manifest;
    this.webapps[id].installTime = (new Date()).getTime()

    this._writeFile(this.appsFile, JSON.stringify(this.webapps), (function() {
      this.mm.sendAsyncMessage("Webapps:Install:Return:OK", aData);
    }).bind(this));
  },
 
  _appId: function(aURI) {
    for (let id in this.webapps) {
      if (this.webapps[id].origin == aURI)
        return id;
    }
    return null;
  },

  _readManifest: function(aId) {
    let file = this.appsDir.clone();
    file.append(aId);
    file.append("manifest.json");
    let data = "";  
    let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
    var cstream = Cc["@mozilla.org/intl/converter-input-stream;1"].createInstance(Ci.nsIConverterInputStream);
    fstream.init(file, -1, 0, 0);
    cstream.init(fstream, "UTF-8", 0, 0);
    let (str = {}) {  
      let read = 0;  
      do {   
        read = cstream.readString(0xffffffff, str); // read as much as we can and put it in str.value  
        data += str.value;  
      } while (read != 0);  
    }  
    cstream.close(); // this closes fstream  
    try {
      return JSON.parse(data);
    } catch(e) {
      return null;
    }
  },
  
  uninstall: function(aData) {
    for (let id in this.webapps) {
      let app = this.webapps[id];
      if (app.origin == aData.origin) {
        delete this.webapps[id];
        this._writeFile(this.appsFile, JSON.stringify(this.webapps));
        let dir = this.appsDir.clone();
        dir.append(id);
        try {
          dir.remove(true);
        } catch (e) {
        }
        this.mm.sendAsyncMessage("Webapps:Uninstall:Return:OK", aData);
      }
    }
  },
  
  enumerate: function(aData) {
    aData.apps = [];

    let id = this._appId(aData.origin);
    // if it's an app, add itself to the result
    if (id) {
      let app = this._cloneAppObject(this.webapps[id]);
      app.manifest = this._readManifest(id);
      aData.apps.push(app);
    }

    // check if it's a store.
    let isStore = false;
    for (id in this.webapps) {
      let app = this._cloneAppObject(this.webapps[id]);
      if (app.installOrigin == aData.origin) {
        isStore = true;
        break;
      }
    }

    // add all the apps from this store
    if (isStore) {
      for (id in this.webapps) {
        let app = this._cloneAppObject(this.webapps[id]);
        if (app.installOrigin == aData.origin) {
          app.manifest = this._readManifest(id);
          aData.apps.push(app);
        }
      }
    }

    this.mm.sendAsyncMessage("Webapps:Enumerate:Return:OK", aData);
  },

  denyEnumerate: function(aData) {
    this.mm.sendAsyncMessage("Webapps:Enumerate:Return:KO", aData);
  },

  enumerateAll: function(aData) {
    aData.apps = [];

    for (id in this.webapps) {
      let app = this._cloneAppObject(this.webapps[id]);
      app.manifest = this._readManifest(id);
      aData.apps.push(app);
    }

    this.mm.sendAsyncMessage("Webapps:Enumerate:Return:OK", aData);
  },

  getManifestFor: function(aOrigin) {
    let id = this._appId(aOrigin);
    if (!id)
      return null;
    return this._readManifest(id);
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
