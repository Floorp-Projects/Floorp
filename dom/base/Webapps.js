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

XPCOMUtils.defineLazyGetter(Services, "rs", function() {
  return Cc["@mozilla.org/dom/dom-request-service;1"].getService(Ci.nsIDOMRequestService);
});

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
});

function convertAppsArray(aApps, aWindow) {
  let apps = new Array();
  for (let i = 0; i < aApps.length; i++) {
    let app = aApps[i];
    apps.push(new WebappsApplication(aWindow, app.origin, app.manifest, app.manifestURL, 
                                     app.receipts, app.installOrigin, app.installTime));
  }
  return apps;
}

function WebappsRegistry() {
}

WebappsRegistry.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  /** from https://developer.mozilla.org/en/OpenWebApps/The_Manifest
   * only the name property is mandatory
   */
  checkManifest: function(aManifest, aInstallOrigin) {
    // TODO : check for install_allowed_from
    if (aManifest.name == undefined)
      return false;

    if (aManifest.installs_allowed_from) {
      ok = false;
      aManifest.installs_allowed_from.forEach(function(aOrigin) {
        if (aOrigin == "*" || aOrigin == aInstallOrigin)
          ok = true;
      });
      return ok;
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
        Services.rs.fireSuccess(req, new WebappsApplication(this._window, app.origin, app.manifest, app.manifestURL, app.receipts,
                                                app.installOrigin, app.installTime));
        break;
      case "Webapps:Install:Return:KO":
        Services.rs.fireError(req, "DENIED");
        break;
      case "Webapps:GetSelf:Return:OK":
        if (msg.apps.length) {
          app = msg.apps[0];
          Services.rs.fireSuccess(req, new WebappsApplication(this._window, app.origin, app.manifest, app.manifestURL, app.receipts,
                                                  app.installOrigin, app.installTime));
        } else {
          Services.rs.fireSuccess(req, null);
        }
        break;
      case "Webapps:GetInstalled:Return:OK":
        Services.rs.fireSuccess(req, convertAppsArray(msg.apps, this._window));
        break;
      case "Webapps:GetSelf:Return:KO":
      case "Webapps:GetInstalled:Return:KO":
        Services.rs.fireError(req, "ERROR");
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
    let request = this.createRequest();
    let requestID = this.getRequestId(request);
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", aURL, true);

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200) {
        try {
          let installOrigin = this._getOrigin(this._window.location.href);
          let manifest = JSON.parse(xhr.responseText, installOrigin);
          if (!this.checkManifest(manifest, installOrigin)) {
            Services.rs.fireError(request, "INVALID_MANIFEST");
          } else {
            let receipts = (aParams && aParams.receipts && Array.isArray(aParams.receipts)) ? aParams.receipts : [];
            cpmm.sendAsyncMessage("Webapps:Install", { app: { installOrigin: installOrigin,
                                                              origin: this._getOrigin(aURL),
                                                              manifestURL: aURL,
                                                              manifest: manifest,
                                                              receipts: receipts },
                                                              from: this._window.location.href,
                                                              oid: this._id,
                                                              requestID: requestID });
          }
        } catch(e) {
          Services.rs.fireError(request, "MANIFEST_PARSE_ERROR");
        }
      }
      else {
        Services.rs.fireError(request, "MANIFEST_URL_ERROR");
      }      
    }).bind(this), false);

    xhr.addEventListener("error", (function() {
      Services.rs.fireError(request, "NETWORK_ERROR");
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
                              "Webapps:GetInstalled:Return:OK", "Webapps:GetInstalled:Return:KO",
                              "Webapps:GetSelf:Return:OK", "Webapps:GetSelf:Return:KO"]);

    Services.obs.addObserver(this, "inner-window-destroyed", false);
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
function WebappsApplication(aWindow, aOrigin, aManifest, aManifestURL, aReceipts, aInstallOrigin, aInstallTime) {
  this._origin = aOrigin;
  this._manifest = aManifest;
  this._manifestURL = aManifestURL;
  this._receipts = aReceipts;
  this._installOrigin = aInstallOrigin;
  this._installTime = aInstallTime;

  this.initHelper(aWindow, ["Webapps:Uninstall:Return:OK", "Webapps:Uninstall:Return:KO"]);
}

WebappsApplication.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  _origin: null,
  _manifest: null,
  _manifestURL: null,
  _receipts: [],
  _installOrigin: null,
  _installTime: 0,

  get origin() {
    return this._origin;
  },

  get manifest() {
    return this._manifest;
  },

  get manifestURL() {
    return this._manifestURL;
  },

  get receipts() {
    return this._receipts;
  },

  get installOrigin() {
    return this._installOrigin;
  },
  
  get installTime() {
    return this._installTime;
  },

  launch: function(aStartPoint) {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:Launch", { origin: this._origin,
                                              startPoint: aStartPoint,
                                              oid: this._id,
                                              requestID: this.getRequestId(request) });
    return request;
  },

  uninstall: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:Uninstall", { origin: this._origin,
                                                 oid: this._id,
                                                 requestID: this.getRequestId(request) });
    return request;
  },

  receiveMessage: function(aMessage) {
    var msg = aMessage.json;
    let req = this.getRequest(msg.requestID);
    if (msg.oid != this._id || !req)
      return;
    switch (aMessage.name) {
      case "Webapps:Uninstall:Return:OK":
        Services.rs.fireSuccess(req, msg.origin);
        break;
      case "Webapps:Uninstall:Return:KO":
        Services.rs.fireError(req, msg.origin);
        break;
    }
    this.removeRequest(msg.requestID);
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

  let perm = principal == secMan.getSystemPrincipal() ? 
               Ci.nsIPermissionManager.ALLOW_ACTION : 
               Services.perms.testExactPermission(principal.URI, "webapps-manage");

  //only pages with perm set can use some functions
  this.hasPrivileges = perm == Ci.nsIPermissionManager.ALLOW_ACTION;

  this.initHelper(aWindow, ["Webapps:GetAll:Return:OK", "Webapps:GetAll:Return:KO",
                            "Webapps:Install:Return:OK", "Webapps:Uninstall:Return:OK"]);

  this._oninstall = null;
  this._onuninstall = null;
}

WebappsApplicationMgmt.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

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
    this._onuninstall;
  },

  set oninstall(aCallback) {
    if (this.hasPrivileges)
      this._oninstall = aCallback;
    else

      throw new Components.exception("Denied", Cr.NS_ERROR_FAILURE);
  },

  set onuninstall(aCallback) {
    if (this.hasPrivileges)
      this._onuninstall = aCallback;
    else
      throw new Components.exception("Denied", Cr.NS_ERROR_FAILURE);
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
        Services.rs.fireSuccess(req, convertAppsArray(msg.apps, this._window));
        break;
      case "Webapps:GetAll:Return:KO":
        Services.rs.fireError(req, "DENIED");
        break;
      case "Webapps:Install:Return:OK":
        if (this._oninstall) {
          let app = msg.app;
          let event = new WebappsApplicationEvent(new WebappsApplication(this._window, app.origin, app.manifest, app.manifestURL, app.receipts,
                                                app.installOrigin, app.installTime));
          this._oninstall.handleEvent(event);
        }
        break;
      case "Webapps:Uninstall:Return:OK":
        if (this._onuninstall) {
          let event = new WebappsApplicationEvent(new WebappsApplication(this._window, msg.origin, null, null, null, null, 0));
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

/**
  * mozIDOMApplicationEvent object
  */
function WebappsApplicationEvent(aApp) {
  this._app = aApp;
}

WebappsApplicationEvent.prototype = {
  get application() {
    return this._app;
  },

  classID: Components.ID("{5bc42b2a-9acc-49d5-a336-c353c8125e48}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.mozIDOMApplicationEvent]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{8c1bca96-266f-493a-8d57-ec7a95098c15}"),
                                    contractID: "@mozilla.org/webapps/application-event;1",
                                    interfaces: [Ci.mozIDOMApplicationEvent],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Webapps Application Event"})
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([WebappsRegistry, WebappsApplication]);
