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
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/BrowserElementPromptService.jsm");
Cu.import("resource://gre/modules/AppsServiceChild.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function debug(aMsg) {
  dump("-*- Webapps.js " + aMsg + "\n");
}

function convertAppsArray(aApps, aWindow) {
  let apps = new aWindow.Array();
  aApps.forEach((aApp) => {
    let obj = createContentApplicationObject(aWindow, aApp);
    apps.push(obj);
  });
  return apps;
}

function WebappsRegistry() {
}

WebappsRegistry.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

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
        this.removeMessageListeners("Webapps:Install:Return:KO");
        Services.DOMRequest.fireSuccess(req, createContentApplicationObject(this._window, app));
        cpmm.sendAsyncMessage("Webapps:Install:Return:Ack",
                              { manifestURL : app.manifestURL });
        break;
      case "Webapps:Install:Return:KO":
        this.removeMessageListeners(aMessage.name);
        Services.DOMRequest.fireError(req, msg.error || "DENIED");
        break;
      case "Webapps:GetSelf:Return:OK":
        this.removeMessageListeners(aMessage.name);
        if (msg.apps.length) {
          app = msg.apps[0];
          Services.DOMRequest.fireSuccess(req, createContentApplicationObject(this._window, app));
        } else {
          Services.DOMRequest.fireSuccess(req, null);
        }
        break;
      case "Webapps:CheckInstalled:Return:OK":
        this.removeMessageListeners(aMessage.name);
        Services.DOMRequest.fireSuccess(req, msg.app);
        break;
      case "Webapps:GetInstalled:Return:OK":
        this.removeMessageListeners(aMessage.name);
        Services.DOMRequest.fireSuccess(req, convertAppsArray(msg.apps, this._window));
        break;
    }
    this.removeRequest(msg.requestID);
  },

  _getOrigin: function(aURL) {
    let uri = Services.io.newURI(aURL, null, null);
    return uri.prePath;
  },

  // Checks that the URL scheme is appropriate (http or https) and
  // asynchronously fire an error on the DOM Request if it isn't.
  _validateURL: function(aURL, aRequest) {
    let uri;
    let res;

    try {
      uri = Services.io.newURI(aURL, null, null);
      if (uri.schemeIs("http") || uri.schemeIs("https")) {
        res = uri.spec;
      }
    } catch(e) {
      Services.DOMRequest.fireErrorAsync(aRequest, "INVALID_URL");
      return false;
    }

    // The scheme is incorrect, fire DOMRequest error.
    if (!res) {
      Services.DOMRequest.fireErrorAsync(aRequest, "INVALID_URL");
      return false;
    }

    return uri.spec;
  },

  // Checks that we run as a foreground page, and fire an error on the
  // DOM Request if we aren't.
  _ensureForeground: function(aRequest) {
    let docShell = this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShell);
    if (docShell.isActive) {
      return true;
    }

    Services.DOMRequest.fireErrorAsync(aRequest, "BACKGROUND_APP");
    return false;
  },

  _prepareInstall: function(aURL, aRequest, aParams, isPackage) {
    let installURL = this._window.location.href;
    let requestID = this.getRequestId(aRequest);
    let receipts = (aParams && aParams.receipts &&
                    Array.isArray(aParams.receipts)) ? aParams.receipts
                                                     : [];
    let categories = (aParams && aParams.categories &&
                      Array.isArray(aParams.categories)) ? aParams.categories
                                                         : [];

    let principal = this._window.document.nodePrincipal;

    return { app: {
                    installOrigin: this._getOrigin(installURL),
                    origin: this._getOrigin(aURL),
                    manifestURL: aURL,
                    receipts: receipts,
                    categories: categories
                  },

             from: installURL,
             oid: this._id,
             requestID: requestID,
             appId: principal.appId,
             isBrowser: principal.isInBrowserElement,
             isPackage: isPackage
           };
  },

  // mozIDOMApplicationRegistry implementation

  install: function(aURL, aParams) {
    let request = this.createRequest();

    let uri = this._validateURL(aURL, request);

    if (uri && this._ensureForeground(request)) {
      this.addMessageListeners("Webapps:Install:Return:KO");
      cpmm.sendAsyncMessage("Webapps:Install",
                            this._prepareInstall(uri, request, aParams, false));
    }

    return request;
  },

  getSelf: function() {
    let request = this.createRequest();
    this.addMessageListeners("Webapps:GetSelf:Return:OK");
    cpmm.sendAsyncMessage("Webapps:GetSelf", { origin: this._getOrigin(this._window.location.href),
                                               appId: this._window.document.nodePrincipal.appId,
                                               oid: this._id,
                                               requestID: this.getRequestId(request) });
    return request;
  },

  checkInstalled: function(aManifestURL) {
    let manifestURL = Services.io.newURI(aManifestURL, null, this._window.document.baseURIObject);

    let request = this.createRequest();

    try {
      this._window.document.nodePrincipal.checkMayLoad(manifestURL, true,
                                                       false);
    } catch (ex) {
      Services.DOMRequest.fireErrorAsync(request, "CROSS_ORIGIN_CHECK_NOT_ALLOWED");
      return request;
    }

    this.addMessageListeners("Webapps:CheckInstalled:Return:OK");
    cpmm.sendAsyncMessage("Webapps:CheckInstalled", { origin: this._getOrigin(this._window.location.href),
                                                      manifestURL: manifestURL.spec,
                                                      oid: this._id,
                                                      requestID: this.getRequestId(request) });
    return request;
  },

  getInstalled: function() {
    let request = this.createRequest();
    this.addMessageListeners("Webapps:GetInstalled:Return:OK");
    cpmm.sendAsyncMessage("Webapps:GetInstalled", { origin: this._getOrigin(this._window.location.href),
                                                    oid: this._id,
                                                    requestID: this.getRequestId(request) });
    return request;
  },

  get mgmt() {
    if (!this.hasMgmtPrivilege) {
      return null;
    }

    if (!this._mgmt) {
      let mgmt = Cc["@mozilla.org/webapps/manager;1"]
                   .createInstance(Ci.nsISupports);
      mgmt.wrappedJSObject.init(this._window);
      this._mgmt = mgmt.__DOM_IMPL__
        ? mgmt.__DOM_IMPL__
        : this._window.DOMApplicationsManager._create(this._window, mgmt.wrappedJSObject);
    }
    return this._mgmt;
  },

  uninit: function() {
    this._mgmt = null;
    cpmm.sendAsyncMessage("Webapps:UnregisterForMessages",
                          ["Webapps:Install:Return:OK"]);
  },

  installPackage: function(aURL, aParams) {
    let request = this.createRequest();

    let uri = this._validateURL(aURL, request);

    if (uri && this._ensureForeground(request)) {
      this.addMessageListeners("Webapps:Install:Return:KO");
      cpmm.sendAsyncMessage("Webapps:InstallPackage",
                            this._prepareInstall(uri, request, aParams, true));
    }

    return request;
  },

  // nsIDOMGlobalPropertyInitializer implementation
  init: function(aWindow) {
    this.initDOMRequestHelper(aWindow, "Webapps:Install:Return:OK");

    let util = this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    this._id = util.outerWindowID;
    cpmm.sendAsyncMessage("Webapps:RegisterForMessages",
                          { messages: ["Webapps:Install:Return:OK"]});

    let principal = aWindow.document.nodePrincipal;
    let perm = Services.perms
               .testExactPermissionFromPrincipal(principal, "webapps-manage");

    // Only pages with the webapps-manage permission set can get access to
    // the mgmt object.
    this.hasMgmtPrivilege = perm == Ci.nsIPermissionManager.ALLOW_ACTION;
  },

  classID: Components.ID("{fff440b3-fae2-45c1-bf03-3b5a2e432270}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsISupports,
                                         Ci.nsIObserver,
                                         Ci.nsIDOMGlobalPropertyInitializer])
}

/**
  * DOMApplication object
  */

function createApplicationObject(aWindow, aApp) {
  let app = Cc["@mozilla.org/webapps/application;1"]
              .createInstance(Ci.nsISupports);
  app.wrappedJSObject.init(aWindow, aApp);
  return app;
}

function createContentApplicationObject(aWindow, aApp) {
  return createApplicationObject(aWindow, aApp).wrappedJSObject
         ._prepareForContent();
}

function WebappsApplication() {
  this.wrappedJSObject = this;
}

WebappsApplication.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  init: function(aWindow, aApp) {
    let proxyHandler = DOMApplicationRegistry.addDOMApp(this,
                                                        aApp.manifestURL,
                                                        aApp.id);
    this._proxy = new Proxy(this, proxyHandler);

    this._window = aWindow;

    this.initDOMRequestHelper(aWindow);
  },

  get _appStatus() {
    return this._proxy.appStatus;
  },

  get downloadAvailable() {
    return this._proxy.downloadAvailable;
  },

  get downloading() {
    return this._proxy.downloading;
  },

  get downloadSize() {
    return this._proxy.downloadSize;
  },

  get installOrigin() {
    return this._proxy.installOrigin;
  },

  get installState() {
    return this._proxy.installState;
  },

  get installTime() {
    return this._proxy.installTime;
  },

  get lastUpdateCheck() {
    return this._proxy.lastUpdateCheck;
  },

  get manifestURL() {
    return this._proxy.manifestURL;
  },

  get origin() {
    return this._proxy.origin;
  },

  get progress() {
    return this._proxy.progress;
  },

  get readyToApplyDownload() {
    return this._proxy.readyToApplyDownload;
  },

  get removable() {
    return this._proxy.removable;
  },

  get updateTime() {
    return this._proxy.updateTime;
  },

  get manifest() {
    return WrappedManifestCache.get(this.manifestURL,
                                    this._proxy.manifest,
                                    this._window,
                                    this.innerWindowID);
  },

  get updateManifest() {
    return this._proxy.updateManifest ?
      Cu.cloneInto(this._proxy.updateManifest, this._window) : null;
  },

  set onprogress(aCallback) {
    this.__DOM_IMPL__.setEventHandler("onprogress", aCallback);
  },

  get onprogress() {
    return this.__DOM_IMPL__.getEventHandler("onprogress");
  },

  set ondownloadsuccess(aCallback) {
    this.__DOM_IMPL__.setEventHandler("ondownloadsuccess", aCallback);
  },

  get ondownloadsuccess() {
    return this.__DOM_IMPL__.getEventHandler("ondownloadsuccess");
  },

  set ondownloaderror(aCallback) {
    this.__DOM_IMPL__.setEventHandler("ondownloaderror", aCallback);
  },

  get ondownloaderror() {
    return this.__DOM_IMPL__.getEventHandler("ondownloaderror");
  },

  set ondownloadavailable(aCallback) {
    this.__DOM_IMPL__.setEventHandler("ondownloadavailable", aCallback);
  },

  get ondownloadavailable() {
    return this.__DOM_IMPL__.getEventHandler("ondownloadavailable");
  },

  set ondownloadapplied(aCallback) {
    this.__DOM_IMPL__.setEventHandler("ondownloadapplied", aCallback);
  },

  get ondownloadapplied() {
    return this.__DOM_IMPL__.getEventHandler("ondownloadapplied");
  },

  get receipts() {
    return this._proxy.receipts;
  },

  get downloadError() {
    // Only return DOMError when we have an error.
    if (!this._proxy.downloadError) {
      return null;
    }
    return new this._window.DOMError(this._proxy.downloadError);
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
    this.addMessageListeners(["Webapps:Launch:Return:OK",
                              "Webapps:Launch:Return:KO"]);
    cpmm.sendAsyncMessage("Webapps:Launch", { origin: this.origin,
                                              manifestURL: this.manifestURL,
                                              startPoint: aStartPoint || "",
                                              oid: this._id,
                                              timestamp: Date.now(),
                                              requestID: this.getRequestId(request) });
    return request;
  },

  clearBrowserData: function() {
    let request = this.createRequest();
    let browserChild =
      BrowserElementPromptService.getBrowserElementChildForWindow(this._window);
    if (browserChild) {
      this.addMessageListeners("Webapps:ClearBrowserData:Return");
      browserChild.messageManager.sendAsyncMessage("Webapps:ClearBrowserData", {
        manifestURL: this.manifestURL,
        oid: this._id,
        requestID: this.getRequestId(request)
      });
    } else {
      Services.DOMRequest.fireErrorAsync(request, "NO_CLEARABLE_BROWSER");
    }
    return request;
  },

  connect: function(aKeyword, aRules) {
    this.addMessageListeners(["Webapps:Connect:Return:OK",
                              "Webapps:Connect:Return:KO"]);
    return this.createPromise(function (aResolve, aReject) {
      cpmm.sendAsyncMessage("Webapps:Connect", {
        keyword: aKeyword,
        rules: aRules,
        manifestURL: this.manifestURL,
        outerWindowID: this._id,
        requestID: this.getPromiseResolverId({
          resolve: aResolve,
          reject: aReject
        })
      });
    }.bind(this));
  },

  getConnections: function() {
    this.addMessageListeners("Webapps:GetConnections:Return:OK");
    return this.createPromise(function (aResolve, aReject) {
      cpmm.sendAsyncMessage("Webapps:GetConnections", {
        manifestURL: this.manifestURL,
        outerWindowID: this._id,
        requestID: this.getPromiseResolverId({
          resolve: aResolve,
          reject: aReject
        })
      });
    }.bind(this));
  },

  addReceipt: function(receipt) {
    let request = this.createRequest();

    this.addMessageListeners(["Webapps:AddReceipt:Return:OK",
                              "Webapps:AddReceipt:Return:KO"]);

    cpmm.sendAsyncMessage("Webapps:AddReceipt", { manifestURL: this.manifestURL,
                                                  receipt: receipt,
                                                  oid: this._id,
                                                  requestID: this.getRequestId(request) });

    return request;
  },

  removeReceipt: function(receipt) {
    let request = this.createRequest();

    this.addMessageListeners(["Webapps:RemoveReceipt:Return:OK",
                              "Webapps:RemoveReceipt:Return:KO"]);

    cpmm.sendAsyncMessage("Webapps:RemoveReceipt", { manifestURL: this.manifestURL,
                                                     receipt: receipt,
                                                     oid: this._id,
                                                     requestID: this.getRequestId(request) });

    return request;
  },

  replaceReceipt: function(oldReceipt, newReceipt) {
    let request = this.createRequest();

    this.addMessageListeners(["Webapps:ReplaceReceipt:Return:OK",
                              "Webapps:ReplaceReceipt:Return:KO"]);

    cpmm.sendAsyncMessage("Webapps:ReplaceReceipt", { manifestURL: this.manifestURL,
                                                      newReceipt: newReceipt,
                                                      oldReceipt: oldReceipt,
                                                      oid: this._id,
                                                      requestID: this.getRequestId(request) });

    return request;
  },

  _prepareForContent: function() {
    if (this.__DOM_IMPL__) {
      return this.__DOM_IMPL__;
    }
    return this._window.DOMApplication._create(this._window, this.wrappedJSObject);
  },

  uninit: function() {
    WrappedManifestCache.evict(this.manifestURL, this.innerWindowID);
  },

  _fireEvent: function(aName) {
    let obj = this._prepareForContent();
    let event = new this._window.MozApplicationEvent(aName, {
      application: obj
    });
    obj.dispatchEvent(event);
  },

  _fireRequestResult: function(aMessage, aIsError) {
    let req;
    let msg = aMessage.data;
    req = this.takeRequest(msg.requestID);
    if (!req) {
      return;
    }

    aIsError ? Services.DOMRequest.fireError(req, msg.error)
             : Services.DOMRequest.fireSuccess(req, msg.result);
  },

  receiveMessage: function(aMessage) {
    let msg = aMessage.json;
    let req;
    if (aMessage.name == "Webapps:Connect:Return:OK" ||
        aMessage.name == "Webapps:Connect:Return:KO" ||
        aMessage.name == "Webapps:GetConnections:Return:OK") {
      req = this.takePromiseResolver(msg.requestID);
    } else {
      req = this.takeRequest(msg.requestID);
    }

    if (msg.oid !== this._id || !req) {
      return;
    }

    switch (aMessage.name) {
      case "Webapps:Launch:Return:KO":
        this.removeMessageListeners(["Webapps:Launch:Return:OK",
                                     "Webapps:Launch:Return:KO"]);
        Services.DOMRequest.fireError(req, "APP_INSTALL_PENDING");
        break;
      case "Webapps:Launch:Return:OK":
        this.removeMessageListeners(["Webapps:Launch:Return:OK",
                                     "Webapps:Launch:Return:KO"]);
        Services.DOMRequest.fireSuccess(req, null);
        break;
      case "Webapps:ClearBrowserData:Return":
        this.removeMessageListeners(aMessage.name);
        Services.DOMRequest.fireSuccess(req, null);
        break;
      case "Webapps:Connect:Return:OK":
        this.removeMessageListeners(["Webapps:Connect:Return:OK",
                                     "Webapps:Connect:Return:KO"]);
        let messagePorts = [];
        msg.messagePortIDs.forEach((aPortID) => {
          let port = new this._window.MozInterAppMessagePort(aPortID);
          messagePorts.push(port);
        });
        req.resolve(messagePorts);
        break;
      case "Webapps:Connect:Return:KO":
        this.removeMessageListeners(["Webapps:Connect:Return:OK",
                                     "Webapps:Connect:Return:KO"]);
        req.reject("No connections registered");
        break;
      case "Webapps:GetConnections:Return:OK":
        this.removeMessageListeners(aMessage.name);
        let connections = [];
        msg.connections.forEach((aConnection) => {
          let connection =
            new this._window.MozInterAppConnection(aConnection.keyword,
                                                   aConnection.pubAppManifestURL,
                                                   aConnection.subAppManifestURL);
          connections.push(connection);
        });
        req.resolve(connections);
        break;
      case "Webapps:AddReceipt:Return:OK":
        this.removeMessageListeners(["Webapps:AddReceipt:Return:OK",
                                     "Webapps:AddReceipt:Return:KO"]);
        this._proxy.receipts = msg.receipts;
        Services.DOMRequest.fireSuccess(req, null);
        break;
      case "Webapps:AddReceipt:Return:KO":
        this.removeMessageListeners(["Webapps:AddReceipt:Return:OK",
                                     "Webapps:AddReceipt:Return:KO"]);
        Services.DOMRequest.fireError(req, msg.error);
        break;
      case "Webapps:RemoveReceipt:Return:OK":
        this.removeMessageListeners(["Webapps:RemoveReceipt:Return:OK",
                                     "Webapps:RemoveReceipt:Return:KO"]);
        this._proxy.receipts = msg.receipts;
        Services.DOMRequest.fireSuccess(req, null);
        break;
      case "Webapps:RemoveReceipt:Return:KO":
        this.removeMessageListeners(["Webapps:RemoveReceipt:Return:OK",
                                     "Webapps:RemoveReceipt:Return:KO"]);
        Services.DOMRequest.fireError(req, msg.error);
        break;
      case "Webapps:ReplaceReceipt:Return:OK":
        this.removeMessageListeners(["Webapps:ReplaceReceipt:Return:OK",
                                     "Webapps:ReplaceReceipt:Return:KO"]);
        this._proxy.receipts = msg.receipts;
        Services.DOMRequest.fireSuccess(req, null);
        break;
      case "Webapps:ReplaceReceipt:Return:KO":
        this.removeMessageListeners(["Webapps:ReplaceReceipt:Return:OK",
                                     "Webapps:ReplaceReceipt:Return:KO"]);
        Services.DOMRequest.fireError(req, msg.error);
        break;
    }
  },

  classID: Components.ID("{723ed303-7757-4fb0-b261-4f78b1f6bd22}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
}

/**
  * DOMApplicationsManager object
  */
function WebappsApplicationMgmt() {
  this.wrappedJSObject = this;
}

WebappsApplicationMgmt.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  init: function(aWindow) {
    this.initDOMRequestHelper(aWindow, ["Webapps:Uninstall:Return:OK",
                                        "Webapps:Uninstall:Broadcast:Return:OK",
                                        "Webapps:Uninstall:Return:KO",
                                        "Webapps:Install:Return:OK",
                                        "Webapps:GetNotInstalled:Return:OK"]);
    cpmm.sendAsyncMessage("Webapps:RegisterForMessages",
                          {
                            messages: ["Webapps:Install:Return:OK",
                                       "Webapps:Uninstall:Return:OK",
                                       "Webapps:Uninstall:Broadcast:Return:OK"]
                          }
                         );
  },

  uninit: function() {
    cpmm.sendAsyncMessage("Webapps:UnregisterForMessages",
                          ["Webapps:Install:Return:OK",
                           "Webapps:Uninstall:Return:OK",
                           "Webapps:Uninstall:Broadcast:Return:OK"]);
  },

  applyDownload: function(aApp) {
    if (!aApp.readyToApplyDownload) {
      return;
    }

    cpmm.sendAsyncMessage("Webapps:ApplyDownload",
                          { manifestURL: aApp.manifestURL });
  },

  uninstall: function(aApp) {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:Uninstall", { origin: aApp.origin,
                                                 manifestURL: aApp.manifestURL,
                                                 oid: this._id,
                                                 requestID: this.getRequestId(request) });
    return request;
  },

  getAll: function() {
    let request = this.createRequest();
    let window = this._window;
    DOMApplicationRegistry.getAll((aApps) => {
      Services.DOMRequest.fireSuccessAsync(request,
                                           convertAppsArray(aApps, window));
    });

    return request;
  },

  getNotInstalled: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:GetNotInstalled", { oid: this._id,
                                                       requestID: this.getRequestId(request) });
    return request;
  },

  get oninstall() {
    return this.__DOM_IMPL__.getEventHandler("oninstall");
  },

  get onuninstall() {
    return this.__DOM_IMPL__.getEventHandler("onuninstall");
  },

  set oninstall(aCallback) {
    this.__DOM_IMPL__.setEventHandler("oninstall", aCallback);
  },

  set onuninstall(aCallback) {
    this.__DOM_IMPL__.setEventHandler("onuninstall", aCallback);
  },

  receiveMessage: function(aMessage) {
    var msg = aMessage.json;
    let req = this.getRequest(msg.requestID);
    // We want Webapps:Install:Return:OK and Webapps:Uninstall:Broadcast:Return:OK
    // to be broadcasted to all instances of mozApps.mgmt.
    if (!((msg.oid == this._id && req) ||
          aMessage.name == "Webapps:Install:Return:OK" ||
          aMessage.name == "Webapps:Uninstall:Broadcast:Return:OK")) {
      return;
    }
    switch (aMessage.name) {
      case "Webapps:GetNotInstalled:Return:OK":
        Services.DOMRequest.fireSuccess(req, convertAppsArray(msg.apps, this._window));
        break;
      case "Webapps:Install:Return:OK":
        {
          let app = createContentApplicationObject(this._window, msg.app);
          let event =
            new this._window.MozApplicationEvent("install", { application: app });
          this.__DOM_IMPL__.dispatchEvent(event);
        }
        break;
      case "Webapps:Uninstall:Broadcast:Return:OK":
        {
          let app = createContentApplicationObject(this._window, msg);
          let event =
            new this._window.MozApplicationEvent("uninstall", { application : app });
          this.__DOM_IMPL__.dispatchEvent(event);
        }
        break;
      case "Webapps:Uninstall:Return:OK":
        Services.DOMRequest.fireSuccess(req, msg.manifestURL);
        break;
      case "Webapps:Uninstall:Return:KO":
        Services.DOMRequest.fireError(req, "NOT_INSTALLED");
        break;
    }
    if (aMessage.name !== "Webapps:Uninstall:Broadcast:Return:OK") {
      this.removeRequest(msg.requestID);
    }
  },

  classID: Components.ID("{8c1bca96-266f-493a-8d57-ec7a95098c15}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WebappsRegistry,
                                                     WebappsApplicationMgmt,
                                                     WebappsApplication]);
