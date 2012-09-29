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
Cu.import("resource://gre/modules/ObjectWrapper.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/BrowserElementPromptService.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function convertAppsArray(aApps, aWindow) {
  let apps = Cu.createArrayIn(aWindow);
  for (let i = 0; i < aApps.length; i++) {
    let app = aApps[i];
    apps.push(createApplicationObject(aWindow, app));
  }

  return apps;
}

function WebappsRegistry() {
}

WebappsRegistry.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  // Hosted apps can't be trusted or certified, so just check that the
  // manifest doesn't ask for those.
  checkAppStatus: function(aManifest) {
    let manifestStatus = aManifest.type || "web";
    return (Services.prefs.getBoolPref("dom.mozApps.dev_mode") ||
            manifestStatus === "web");
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
        Services.DOMRequest.fireSuccess(req, createApplicationObject(this._window, app));
        break;
      case "Webapps:Install:Return:KO":
        Services.DOMRequest.fireError(req, msg.error || "DENIED");
        break;
      case "Webapps:GetSelf:Return:OK":
        if (msg.apps.length) {
          app = msg.apps[0];
          Services.DOMRequest.fireSuccess(req, createApplicationObject(this._window, app));
        } else {
          Services.DOMRequest.fireSuccess(req, null);
        }
        break;
      case "Webapps:IsInstalled:Return:OK":
        Services.DOMRequest.fireSuccess(req, msg.installed);
        break;
      case "Webapps:GetInstalled:Return:OK":
        Services.DOMRequest.fireSuccess(req, convertAppsArray(msg.apps, this._window));
        break;
    }
    this.removeRequest(msg.requestID);
  },

  _getOrigin: function(aURL) {
    let uri = Services.io.newURI(aURL, null, null);
    return uri.prePath;
  },

  _validateScheme: function(aURL) {
    let scheme = Services.io.newURI(aURL, null, null).scheme;
    if (scheme != "http" && scheme != "https") {
      throw new Components.Exception(
        "INVALID_URL_SCHEME: '" + scheme + "'; must be 'http' or 'https'",
        Cr.NS_ERROR_FAILURE
      );
    }
  },

  // mozIDOMApplicationRegistry implementation

  install: function(aURL, aParams) {
    this._validateScheme(aURL);

    let installURL = this._window.location.href;
    let installOrigin = this._getOrigin(installURL);
    let request = this.createRequest();
    let requestID = this.getRequestId(request);
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", aURL, true);
    xhr.channel.loadFlags |= Ci.nsIRequest.VALIDATE_ALWAYS;

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200) {
        let manifest;
        try {
          manifest = JSON.parse(xhr.responseText, installOrigin);
        } catch (e) {
          Services.DOMRequest.fireError(request, "MANIFEST_PARSE_ERROR");
          Cu.reportError("Error installing app from: " + installOrigin + ": " + "MANIFEST_PARSE_ERROR");
          return;
        }

        if (!AppsUtils.checkManifest(manifest, installOrigin)) {
          Services.DOMRequest.fireError(request, "INVALID_MANIFEST");
          Cu.reportError("Error installing app from: " + installOrigin + ": " + "INVALID_MANIFEST");
        } else if (!this.checkAppStatus(manifest)) {
          Services.DOMRequest.fireError(request, "INVALID_SECURITY_LEVEL");
          Cu.reportError("Error installing app, '" + manifest.name + "': " + "INVALID_SECURITY_LEVEL");
        } else {
          let receipts = (aParams && aParams.receipts && Array.isArray(aParams.receipts)) ? aParams.receipts : [];
          let categories = (aParams && aParams.categories && Array.isArray(aParams.categories)) ? aParams.categories : [];
          let etag = xhr.getResponseHeader("Etag");
          cpmm.sendAsyncMessage("Webapps:Install", { app: { installOrigin: installOrigin,
                                                            origin: this._getOrigin(aURL),
                                                            manifestURL: aURL,
                                                            manifest: manifest,
                                                            etag: etag,
                                                            receipts: receipts,
                                                            categories: categories },
                                                            from: installURL,
                                                            oid: this._id,
                                                            requestID: requestID });
        }
      } else {
        Services.DOMRequest.fireError(request, "MANIFEST_URL_ERROR");
        Cu.reportError("Error installing app from: " + installOrigin + ": " + "MANIFEST_URL_ERROR");
      }
    }).bind(this), false);

    xhr.addEventListener("error", (function() {
      Services.DOMRequest.fireError(request, "NETWORK_ERROR");
      Cu.reportError("Error installing app from: " + installOrigin + ": " + "NETWORK_ERROR");
    }).bind(this), false);

    xhr.send(null);
    return request;
  },

  getSelf: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:GetSelf", { origin: this._getOrigin(this._window.location.href),
                                               appId: this._window.document.nodePrincipal.appId,
                                               oid: this._id,
                                               requestID: this.getRequestId(request) });
    return request;
  },

  isInstalled: function(aManifestURL) {
    let manifestURL = Services.io.newURI(aManifestURL, null, this._window.document.baseURIObject);
    this._window.document.nodePrincipal.checkMayLoad(manifestURL, true, false);

    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:IsInstalled", { origin: this._getOrigin(this._window.location.href),
                                                   manifestURL: manifestURL.spec,
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
    cpmm.sendAsyncMessage("Webapps:UnregisterForMessages",
                          ["Webapps:Install:Return:OK"]);
  },

  // mozIDOMApplicationRegistry2 implementation

  installPackage: function(aURL, aParams) {
    let installURL = this._window.location.href;
    let installOrigin = this._getOrigin(installURL);
    this._validateScheme(aURL);

    let request = this.createRequest();
    let requestID = this.getRequestId(request);

    let receipts = (aParams && aParams.receipts &&
                    Array.isArray(aParams.receipts)) ? aParams.receipts : [];
    let categories = (aParams && aParams.categories &&
                      Array.isArray(aParams.categories)) ? aParams.categories : [];
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", aURL, true);
    xhr.channel.loadFlags |= Ci.nsIRequest.VALIDATE_ALWAYS;

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200) {
        let manifest;
        try {
          manifest = JSON.parse(xhr.responseText, installOrigin);
        } catch(e) {
          Services.DOMRequest.fireError(request, "MANIFEST_PARSE_ERROR");
          return;
        }
        if (!(AppsUtils.checkManifest(manifest, installOrigin) &&
              manifest.package_path)) {
          Services.DOMRequest.fireError(request, "INVALID_MANIFEST");
        } else {
          if (!this.checkAppStatus(manifest)) {
            Services.DOMRequest.fireError(request, "INVALID_SECURITY_LEVEL");
          } else {
            let receipts = (aParams && aParams.receipts && Array.isArray(aParams.receipts)) ? aParams.receipts : [];
            let categories = (aParams && aParams.categories && Array.isArray(aParams.categories)) ? aParams.categories : [];
            let etag = xhr.getResponseHeader("Etag");
            cpmm.sendAsyncMessage("Webapps:InstallPackage", { app: {
                                                              installOrigin: installOrigin,
                                                              origin: this._getOrigin(aURL),
                                                              manifestURL: aURL,
                                                              updateManifest: manifest,
                                                              etag: etag,
                                                              receipts: receipts,
                                                              categories: categories },
                                                              from: installURL,
                                                              oid: this._id,
                                                              requestID: requestID,
                                                              isPackage: true });
          }
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

  // nsIDOMGlobalPropertyInitializer implementation
  init: function(aWindow) {
    this.initHelper(aWindow, ["Webapps:Install:Return:OK", "Webapps:Install:Return:KO",
                              "Webapps:GetInstalled:Return:OK",
                              "Webapps:GetSelf:Return:OK",
                              "Webapps:IsInstalled:Return:OK" ]);

    let util = this._window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    this._id = util.outerWindowID;
    cpmm.sendAsyncMessage("Webapps:RegisterForMessages",
                          ["Webapps:Install:Return:OK"]);
  },

  classID: Components.ID("{fff440b3-fae2-45c1-bf03-3b5a2e432270}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.mozIDOMApplicationRegistry,
#ifdef MOZ_PHOENIX
# Firefox Desktop: installPackage not implemented
#elifdef ANDROID
#ifndef MOZ_WIDGET_GONK
# Firefox Android (Fennec): installPackage not implemented
#else
# B2G Gonk: installPackage implemented
                                         Ci.mozIDOMApplicationRegistry2,
#endif
#else
# B2G Desktop and others: installPackage implementation status varies
                                         Ci.mozIDOMApplicationRegistry2,
#endif
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{fff440b3-fae2-45c1-bf03-3b5a2e432270}"),
                                    contractID: "@mozilla.org/webapps;1",
                                    interfaces: [Ci.mozIDOMApplicationRegistry,
#ifdef MOZ_PHOENIX
# Firefox Desktop: installPackage not implemented
#elifdef ANDROID
#ifndef MOZ_WIDGET_GONK
# Firefox Android (Fennec): installPackage not implemented
#else
# B2G Gonk: installPackage implemented
                                                 Ci.mozIDOMApplicationRegistry2,
#endif
#else
# B2G Desktop and others: installPackage implementation status varies
                                                 Ci.mozIDOMApplicationRegistry2,
#endif
                                                 ],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Webapps Registry"})
}

/**
  * nsIDOMDOMError object
  */
function createDOMError(aError) {
  let error = Cc["@mozilla.org/dom-error;1"]
                .createInstance(Ci.nsIDOMDOMError);
  error.wrappedJSObject.init(aError);
  return error;
}

function DOMError() {
  this.wrappedJSObject = this;
}

DOMError.prototype = {
  init: function domerror_init(aError) {
    this.name = aError;
  },

  classID: Components.ID("{dcc1d5b7-43d8-4740-9244-b3d8db0f503d}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMDOMError]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{dcc1d5b7-43d8-4740-9244-b3d8db0f503d}"),
                                    contractID: "@mozilla.org/dom-error;1",
                                    interfaces: [Ci.nsIDOMDOMError],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "DOMError object"})
}

/**
  * mozIDOMApplication object
  */

function createApplicationObject(aWindow, aApp) {
  let app = Cc["@mozilla.org/webapps/application;1"].createInstance(Ci.mozIDOMApplication);
  app.wrappedJSObject.init(aWindow, aApp);
  return app;
}

function WebappsApplication() {
  this.wrappedJSObject = this;
}

WebappsApplication.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  init: function(aWindow, aApp) {
    this.origin = aApp.origin;
    this.manifest = ObjectWrapper.wrap(aApp.manifest, aWindow);
    this.updateManifest = aApp.updateManifest ? ObjectWrapper.wrap(aApp.updateManifest, aWindow)
                                              : null;
    this.manifestURL = aApp.manifestURL;
    this.receipts = aApp.receipts;
    this.installOrigin = aApp.installOrigin;
    this.installTime = aApp.installTime;
    this.installState = aApp.installState || "installed";
    this.removable = aApp.removable;
    this.lastUpdateCheck = aApp.lastUpdateCheck ? aApp.lastUpdateCheck
                                                : Date.now();
    this.progress = NaN;
    this.downloadAvailable = aApp.downloadAvailable;
    this.downloading = aApp.downloading;
    this.readyToApplyDownload = aApp.readyToApplyDownload;
    this.downloadSize = aApp.downloadSize || 0;

    this._onprogress = null;
    this._ondownloadsuccess = null;
    this._ondownloaderror = null;
    this._ondownloadavailable = null;
    this._ondownloadapplied = null;

    this._downloadError = null;

    this.initHelper(aWindow, ["Webapps:Uninstall:Return:OK",
                              "Webapps:Uninstall:Return:KO",
                              "Webapps:OfflineCache",
                              "Webapps:CheckForUpdate:Return:OK",
                              "Webapps:CheckForUpdate:Return:KO",
                              "Webapps:PackageEvent"]);
    cpmm.sendAsyncMessage("Webapps:RegisterForMessages",
                          ["Webapps:Uninstall:Return:OK", "Webapps:OfflineCache",
                           "Webapps:PackageEvent"]);
  },

  set onprogress(aCallback) {
    this._onprogress = aCallback;
  },

  get onprogress() {
    return this._onprogress;
  },

  set ondownloadsuccess(aCallback) {
    this._ondownloadsuccess = aCallback;
  },

  get ondownloadsuccess() {
    return this._ondownloadsuccess;
  },

  set ondownloaderror(aCallback) {
    this._ondownloaderror = aCallback;
  },

  get ondownloaderror() {
    return this._ondownloaderror;
  },

  set ondownloadavailable(aCallback) {
    this._ondownloadavailable = aCallback;
  },

  get ondownloadavailable() {
    return this._ondownloadavailable;
  },

  set ondownloadapplied(aCallback) {
    this._ondownloadapplied = aCallback;
  },

  get ondownloadapplied() {
    return this._ondownloadapplied;
  },

  get downloadError() {
    return createDOMError(this._downloadError);
  },

  download: function() {
    cpmm.sendAsyncMessage("Webapps:Download",
                          { manifestURL: this.manifestURL });
  },

  cancelDownload: function() {
    cpmm.sendAsyncMessage("Webapps:CancelDownload",
                          { manifestURL: this.manifestURL });
  },

  checkForUpdate: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:CheckForUpdate",
                          { manifestURL: this.manifestURL,
                            oid: this._id,
                            requestID: this.getRequestId(request) });
    return request;
  },

  launch: function(aStartPoint) {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:Launch", { origin: this.origin,
                                              manifestURL: this.manifestURL,
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

  clearBrowserData: function() {
    let browserChild =
      BrowserElementPromptService.getBrowserElementChildForWindow(this._window);
    if (browserChild) {
      browserChild.messageManager.sendAsyncMessage("Webapps:ClearBrowserData");
    }
  },

  uninit: function() {
    this._onprogress = null;
    cpmm.sendAsyncMessage("Webapps:UnregisterForMessages",
                          ["Webapps:Uninstall:Return:OK", "Webapps:OfflineCache",
                           "Webapps:PackageEvent"]);
  },

  _fireEvent: function(aName, aHandler) {
    if (aHandler) {
      let event = new this._window.MozApplicationEvent(aName, { application: this });
      aHandler.handleEvent(event);
    }
  },

  receiveMessage: function(aMessage) {
    let msg = aMessage.json;
    let req = this.takeRequest(msg.requestID);
    if ((msg.oid != this._id || !req) &&
        aMessage.name !== "Webapps:OfflineCache" &&
        aMessage.name !== "Webapps:PackageEvent")
      return;
    switch (aMessage.name) {
      case "Webapps:Uninstall:Return:OK":
        Services.DOMRequest.fireSuccess(req, msg.origin);
        break;
      case "Webapps:Uninstall:Return:KO":
        Services.DOMRequest.fireError(req, "NOT_INSTALLED");
        break;
      case "Webapps:Launch:Return:KO":
        Services.DOMRequest.fireError(req, "APP_INSTALL_PENDING");
        break;
      case "Webapps:Uninstall:Return:KO":
        Services.DOMRequest.fireError(req, "NOT_INSTALLED");
        break;
      case "Webapps:OfflineCache":
        if (msg.manifest != this.manifestURL)
          return;

        if (installState in msg) {
          this.installState = msg.installState;
          if (this.installState == "installed") {
            this._fireEvent("downloadsuccess", this._ondownloadsuccess);
            this._fireEvent("downloadapplied", this._ondownloadapplied);
          } else {
            this._fireEvent("downloadprogress", this._onprogress);
          }
        } else if (msg.error) {
          this._downloadError = msg.error;
          this._fireEvent("downloaderror", this._ondownloaderror);
        }
        break;
        case "Webapps:CheckForUpdate:Return:OK":
          for (let prop in msg.app) {
            this[prop] = msg.app[prop];
            if (msg.event == "downloadapplied") {
              Services.DOMRequest.fireSuccess(req, this.manifestURL);
              this._fireEvent("downloadapplied", this._ondownloadapplied);
            } else if (msg.event == "downloadavailable") {
              Services.DOMRequest.fireSuccess(req, this.manifestURL);
              this._fireEvent("downloadavailable", this._ondownloadavailable);
            }
          }
          break;
        case "Webapps:CheckForUpdate:Return:KO":
          Services.DOMRequest.fireError(req, msg.error);
          break;
        case "Webapps:PackageEvent":
          if (msg.manifestURL != this.manifestURL)
            return;
          switch(msg.type) {
            case "error":
              this._downloadError = msg.error;
              this._fireEvent("downloaderror", this._ondownloaderror);
              break;
            case "progress":
              this.progress = msg.progress;
              this._fireEvent("downloadprogress", this._onprogress);
              break;
            case "installed":
              let app = msg.app;
              this.progress = app.progress || 0;
              this.downloadAvailable = app.downloadAvailable;
              this.downloading = app.downloading;
              this.readyToApplyDownload = app.readyToApplyDownload;
              this.downloadSize = app.downloadSize || 0;
              this.installState = app.installState;
              this.manifest = app.manifest;
              this._fireEvent("downloaded", this._ondownloaded);
              this._fireEvent("downloadapplied", this._ondownloadapplied);
              break;
            case "canceled":
              app = msg.app;
              this.progress = app.progress || 0;
              this.downloadAvailable = app.downloadAvailable;
              this.downloading = app.downloading;
              this.readyToApplyDownload = app.readyToApplyDownload;
              this.downloadSize = app.downloadSize || 0;
              this.installState = app.installState;
              this._downloadError = msg.error;
              this._fireEvent("downloaderror", this._ondownloaderror);
              break;
            case "downloaded":
              app = msg.app;
              this.downloading = app.downloading;
              this.downloadavailable = app.downloadavailable;
              this.readyToApplyDownload = app.readyToApplyDownload;
              this._fireEvent("downloaded", this._ondownloaded);
              break;
            case "applied":
              app = msg.app;
              this.readyToApplyDownload = app.readyToApplyDownload;
              this._fireEvent("downloadapplied", this._ondownloadapplied);
              break;
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
                            "Webapps:Install:Return:OK", "Webapps:Uninstall:Return:OK",
                            "Webapps:GetNotInstalled:Return:OK"]);

  cpmm.sendAsyncMessage("Webapps:RegisterForMessages",
                        ["Webapps:Install:Return:OK", "Webapps:Uninstall:Return:OK"]);

  this._oninstall = null;
  this._onuninstall = null;
}

WebappsApplicationMgmt.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  __exposedProps__: {
                      applyDownload: "r",
                      getAll: "r",
                      getNotInstalled: "r",
                      oninstall: "rw",
                      onuninstall: "rw"
                     },

  uninit: function() {
    this._oninstall = null;
    this._onuninstall = null;
    cpmm.sendAsyncMessage("Webapps:UnregisterForMessages",
                          ["Webapps:Install:Return:OK", "Webapps:Uninstall:Return:OK"]);
  },

  applyDownload: function(aApp) {
    if (!aApp.readyToApplyDownload) {
      return;
    }

    cpmm.sendAsyncMessage("Webapps::ApplyDownload",
                          { manifestURL: aApp.manifestURL });
  },

  getAll: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:GetAll", { oid: this._id,
                                              requestID: this.getRequestId(request),
                                              hasPrivileges: this.hasPrivileges });
    return request;
  },

  getNotInstalled: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:GetNotInstalled", { oid: this._id,
                                                       requestID: this.getRequestId(request) });
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
      case "Webapps:GetNotInstalled:Return:OK":
        Services.DOMRequest.fireSuccess(req, convertAppsArray(msg.apps, this._window));
        break;
      case "Webapps:Install:Return:OK":
        if (this._oninstall) {
          let app = msg.app;
          let event = new this._window.MozApplicationEvent("applicationinstall",
                           { application : createApplicationObject(this._window, app) });
          this._oninstall.handleEvent(event);
        }
        break;
      case "Webapps:Uninstall:Return:OK":
        if (this._onuninstall) {
          let event = new this._window.MozApplicationEvent("applicationuninstall",
                           { application : createApplicationObject(this._window, { origin: msg.origin }) });
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

const NSGetFactory = XPCOMUtils.generateNSGetFactory([WebappsRegistry,
                                                      WebappsApplication,
                                                      DOMError]);
