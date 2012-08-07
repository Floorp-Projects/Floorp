/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
});

// Makes sure that we expose correctly chrome JS objects to content.
function wrapObjectIn(aObject, aCtxt) {
  let res = Cu.createObjectIn(aCtxt);
  let propList = { };
  for (let prop in aObject) {
    propList[prop] = {
      enumerable: true,
      configurable: true,
      writable: true,
      value: (typeof(aObject[prop]) == "object") ? wrapObjectIn(aObject[prop], aCtxt)
                                                 : aObject[prop]
    }
  }
  Object.defineProperties(res, propList);
  Cu.makeObjectPropsNormal(res);
  return res;
};

function convertAppsArray(aApps, aWindow) {
  let apps = Cu.createArrayIn(aWindow);
  for (let i = 0; i < aApps.length; i++) {
    let app = aApps[i];
    apps.push(createApplicationObject(aWindow, app.origin, app.manifest, app.manifestURL, 
                                      app.receipts, app.installOrigin, app.installTime));
  }

  return apps;
}

function WebappsRegistry() {
}

WebappsRegistry.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  __exposedProps__: {
                      install: 'r',
                      installPackage: 'r',
                      getSelf: 'r',
                      getInstalled: 'r',
                      getNotInstalled: 'r',
                      mgmt: 'r'
                     },

  /** from https://developer.mozilla.org/en/OpenWebApps/The_Manifest
   * only the name property is mandatory
   */
  checkManifest: function(aManifest, aInstallOrigin) {
    if (aManifest.name == undefined)
      return false;

    if (aManifest.installs_allowed_from) {
      return aManifest.installs_allowed_from.some(function(aOrigin) {
        return aOrigin == "*" || aOrigin == aInstallOrigin;
      });
    }
    return true;
  },

  receiveMessage: function(aMessage) {
    let msg = aMessage.json;
    if (msg.oid != this._id)
      return
    let req = this.getRequest(msg.requestID);
    if (!req)
      return;
    let app = msg.app;
    switch (aMessage.name) {
      case "Webapps:Install:Return:OK":
        Services.DOMRequest.fireSuccess(req, createApplicationObject(this._window, app.origin, app.manifest, app.manifestURL, app.receipts,
                                                                     app.installOrigin, app.installTime));
        break;
      case "Webapps:Install:Return:KO":
        Services.DOMRequest.fireError(req, msg.error || "DENIED");
        break;
      case "Webapps:GetSelf:Return:OK":
        if (msg.apps.length) {
          app = msg.apps[0];
          Services.DOMRequest.fireSuccess(req, createApplicationObject(this._window, app.origin, app.manifest, app.manifestURL, app.receipts,
                                                                       app.installOrigin, app.installTime));
        } else {
          Services.DOMRequest.fireSuccess(req, null);
        }
        break;
      case "Webapps:GetInstalled:Return:OK":
        Services.DOMRequest.fireSuccess(req, convertAppsArray(msg.apps, this._window));
        break;
      case "Webapps:GetNotInstalled:Return:OK":
        Services.DOMRequest.fireSuccess(req, convertAppsArray(msg.apps, this._window));
        break;
      case "Webapps:GetSelf:Return:KO":
        Services.DOMRequest.fireError(req, "ERROR");
        break;
    }
    this.removeRequest(msg.requestID);
  },

  _getOrigin: function(aURL) {
    let uri = Services.io.newURI(aURL, null, null);
    return uri.prePath; 
  },

  // mozIDOMApplicationRegistry implementation
  
  install: function(aURL, aParams) {
    let installURL = this._window.location.href;
    let installOrigin = this._getOrigin(installURL);
    let request = this.createRequest();
    let requestID = this.getRequestId(request);
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", aURL, true);
    xhr.channel.loadFlags |= Ci.nsIRequest.VALIDATE_ALWAYS;

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200) {
        try {
          let manifest = JSON.parse(xhr.responseText, installOrigin);
          if (!this.checkManifest(manifest, installOrigin)) {
            Services.DOMRequest.fireError(request, "INVALID_MANIFEST");
          } else {
            let receipts = (aParams && aParams.receipts && Array.isArray(aParams.receipts)) ? aParams.receipts : [];
            let categories = (aParams && aParams.categories && Array.isArray(aParams.categories)) ? aParams.categories : [];
            cpmm.sendAsyncMessage("Webapps:Install", { app: { installOrigin: installOrigin,
                                                              origin: this._getOrigin(aURL),
                                                              manifestURL: aURL,
                                                              manifest: manifest,
                                                              receipts: receipts,
                                                              categories: categories },
                                                              from: installURL,
                                                              oid: this._id,
                                                              requestID: requestID });
          }
        } catch(e) {
          Services.DOMRequest.fireError(request, "MANIFEST_PARSE_ERROR");
        }
      }
      else {
        Services.DOMRequest.fireError(request, "MANIFEST_URL_ERROR");
      }      
    }).bind(this), false);

    xhr.addEventListener("error", (function() {
      Services.DOMRequest.fireError(request, "NETWORK_ERROR");
    }).bind(this), false);

    xhr.send(null);
    return request;
  },

  getSelf: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:GetSelf", { origin: this._getOrigin(this._window.location.href),
                                               oid: this._id,
                                               requestID: this.getRequestId(request) });
    return request;
  },

  getInstalled: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:GetInstalled", { origin: this._getOrigin(this._window.location.href),
                                                    oid: this._id,
                                                    requestID: this.getRequestId(request) });
    return request;
  },

  getNotInstalled: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:GetNotInstalled", { origin: this._getOrigin(this._window.location.href),
                                                       oid: this._id,
                                                       requestID: this.getRequestId(request) });
    return request;
  },

  installPackage: function(aPackageURL, aParams) {
    let request = this.createRequest();
    let requestID = this.getRequestId(request);

    let receipts = (aParams && aParams.receipts &&
                    Array.isArray(aParams.receipts)) ? aParams.receipts : [];
    let categories = (aParams && aParams.categories &&
                      Array.isArray(aParams.categories)) ? aParams.categories : [];
    cpmm.sendAsyncMessage("Webapps:InstallPackage", { url: aPackageURL,
                                                      receipts: receipts,
                                                      categories: categories,
                                                      requestID: requestID,
                                                      oid: this._id,
                                                      from: this._window.location.href,
                                                      installOrigin: this._getOrigin(this._window.location.href) });
    return request;
  },

  get mgmt() {
    if (!this._mgmt)
      this._mgmt = new WebappsApplicationMgmt(this._window);
    return this._mgmt;
  },

  uninit: function() {
    this._mgmt = null;
  },

  // nsIDOMGlobalPropertyInitializer implementation
  init: function(aWindow) {
    this.initHelper(aWindow, ["Webapps:Install:Return:OK", "Webapps:Install:Return:KO",
                              "Webapps:GetInstalled:Return:OK", "Webapps:GetNotInstalled:Return:OK",
                              "Webapps:GetSelf:Return:OK", "Webapps:GetSelf:Return:KO"]);

    let util = this._window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    this._id = util.outerWindowID;
  },
  
  classID: Components.ID("{fff440b3-fae2-45c1-bf03-3b5a2e432270}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.mozIDOMApplicationRegistry, Ci.nsIDOMGlobalPropertyInitializer]),
  
  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{fff440b3-fae2-45c1-bf03-3b5a2e432270}"),
                                    contractID: "@mozilla.org/webapps;1",
                                    interfaces: [Ci.mozIDOMApplicationRegistry],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Webapps Registry"})
}

/**
  * mozIDOMApplication object
  */

function createApplicationObject(aWindow, aOrigin, aManifest, aManifestURL, aReceipts, aInstallOrigin, aInstallTime) {
  let app = Cc["@mozilla.org/webapps/application;1"].createInstance(Ci.mozIDOMApplication);
  app.wrappedJSObject.init(aWindow, aOrigin, aManifest, aManifestURL, aReceipts, aInstallOrigin, aInstallTime);
  return app;
}

function WebappsApplication() {
  this.wrappedJSObject = this;
}

WebappsApplication.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  __exposedProps__: {
                      origin: 'r',
                      manifest: 'r',
                      manifestURL: 'r',
                      installOrigin: 'r',
                      installTime: 'r',
                      status: 'r',
                      progress: 'r',
                      onprogress: 'rw',
                      launch: 'r',
                      receipts: 'r',
                      uninstall: 'r'
                     },

  init: function(aWindow, aOrigin, aManifest, aManifestURL, aReceipts, aInstallOrigin, aInstallTime) {
    this.origin = aOrigin;
    this.manifest = wrapObjectIn(aManifest, aWindow);
    this.manifestURL = aManifestURL;
    this.receipts = aReceipts;
    this.installOrigin = aInstallOrigin;
    this.installTime = aInstallTime;
    this.status = "installed";
    this.progress = NaN;
    this._onprogress = null;
    this.initHelper(aWindow, ["Webapps:Uninstall:Return:OK", "Webapps:Uninstall:Return:KO", "Webapps:OfflineCache"]);
  },

  set onprogress(aCallback) {
    this._onprogress = aCallback;
  },

  get onprogress() {
    return this._onprogress;
  },

  launch: function(aStartPoint) {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:Launch", { origin: this.origin,
                                              startPoint: aStartPoint || "",
                                              oid: this._id,
                                              requestID: this.getRequestId(request) });
    return request;
  },

  uninstall: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:Uninstall", { origin: this.origin,
                                                 oid: this._id,
                                                 requestID: this.getRequestId(request) });
    return request;
  },

  uninit: function() {
    this._onprogress = null;
  },

  receiveMessage: function(aMessage) {
    var msg = aMessage.json;
    let req = this.takeRequest(msg.requestID);
    if ((msg.oid != this._id || !req) && aMessage.name !== "Webapps:OfflineCache")
      return;
    switch (aMessage.name) {
      case "Webapps:Uninstall:Return:OK":
        Services.DOMRequest.fireSuccess(req, msg.origin);
        break;
      case "Webapps:Uninstall:Return:KO":
        Services.DOMRequest.fireError(req, "NOT_INSTALLED");
        break;
      case "Webapps:OfflineCache":
        if (msg.manifest != this.manifestURL)
          return;
        
        this.status = msg.status;
        if (this._onprogress) {
          let event = new this._window.MozApplicationEvent("applicationinstall", { application: this });
          this._onprogress.handleEvent(event);
        }
        break;
    }
  },

  classID: Components.ID("{723ed303-7757-4fb0-b261-4f78b1f6bd22}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.mozIDOMApplication]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{723ed303-7757-4fb0-b261-4f78b1f6bd22}"),
                                    contractID: "@mozilla.org/webapps/application;1",
                                    interfaces: [Ci.mozIDOMApplication],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Webapps Application"})
}

/**
  * mozIDOMApplicationMgmt object
  */
function WebappsApplicationMgmt(aWindow) {
  let principal = aWindow.document.nodePrincipal;
  let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);

  let perm = principal == secMan.getSystemPrincipal()
               ? Ci.nsIPermissionManager.ALLOW_ACTION
               : Services.perms.testExactPermissionFromPrincipal(principal, "webapps-manage");

  //only pages with perm set can use some functions
  this.hasPrivileges = perm == Ci.nsIPermissionManager.ALLOW_ACTION;

  this.initHelper(aWindow, ["Webapps:GetAll:Return:OK", "Webapps:GetAll:Return:KO",
                            "Webapps:Install:Return:OK", "Webapps:Uninstall:Return:OK"]);

  this._oninstall = null;
  this._onuninstall = null;
}

WebappsApplicationMgmt.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  __exposedProps__: {
                      getAll: 'r',
                      oninstall: 'rw',
                      onuninstall: 'rw'
                     },

  uninit: function() {
    this._oninstall = null;
    this._onuninstall = null;
  },

  getAll: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:GetAll", { oid: this._id,
                                              requestID: this.getRequestId(request),
                                              hasPrivileges: this.hasPrivileges });
    return request;
  },

  get oninstall() {
    return this._oninstall;
  },

  get onuninstall() {
    return this._onuninstall;
  },

  set oninstall(aCallback) {
    if (this.hasPrivileges)
      this._oninstall = aCallback;
    else
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
  },

  set onuninstall(aCallback) {
    if (this.hasPrivileges)
      this._onuninstall = aCallback;
    else
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
  },

  receiveMessage: function(aMessage) {
    var msg = aMessage.json;
    let req = this.getRequest(msg.requestID);
    // We want Webapps:Install:Return:OK and Webapps:Uninstall:Return:OK to be boradcasted
    // to all instances of mozApps.mgmt
    if (!((msg.oid == this._id && req) 
       || aMessage.name == "Webapps:Install:Return:OK" || aMessage.name == "Webapps:Uninstall:Return:OK"))
      return;
    switch (aMessage.name) {
      case "Webapps:GetAll:Return:OK":
        Services.DOMRequest.fireSuccess(req, convertAppsArray(msg.apps, this._window));
        break;
      case "Webapps:GetAll:Return:KO":
        Services.DOMRequest.fireError(req, "DENIED");
        break;
      case "Webapps:Install:Return:OK":
        if (this._oninstall) {
          let app = msg.app;
          let event = new this._window.MozApplicationEvent("applicationinstall", 
                           { application : createApplicationObject(this._window, app.origin, app.manifest, app.manifestURL, app.receipts,
                                                                  app.installOrigin, app.installTime) });
          this._oninstall.handleEvent(event);
        }
        break;
      case "Webapps:Uninstall:Return:OK":
        if (this._onuninstall) {
          let event = new this._window.MozApplicationEvent("applicationuninstall", 
                           { application : createApplicationObject(this._window, msg.origin, null, null, null, null, 0) });
          this._onuninstall.handleEvent(event);
        }
        break;
    }
    this.removeRequest(msg.requestID);
  },

  classID: Components.ID("{8c1bca96-266f-493a-8d57-ec7a95098c15}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.mozIDOMApplicationMgmt]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{8c1bca96-266f-493a-8d57-ec7a95098c15}"),
                                    contractID: "@mozilla.org/webapps/application-mgmt;1",
                                    interfaces: [Ci.mozIDOMApplicationMgmt],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Webapps Application Mgmt"})
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([WebappsRegistry, WebappsApplication]);
