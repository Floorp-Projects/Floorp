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

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function convertAppsArray(aApps, aWindow) {
  let apps = new aWindow.Array();
  for (let i = 0; i < aApps.length; i++) {
    let app = aApps[i];
    // Our application objects are JS-implemented XPCOM objects with DOM_OBJECT
    // set in classinfo. These objects are reflector-per-scope, so as soon as we
    // pass them to content, we'll end up with a new object in content. But from
    // this code, they _appear_ to be chrome objects, and so the Array Xray code
    // vetos the attempt to define a chrome-privileged object on a content Array.
    // Very carefully waive Xrays so that this can keep working until we convert
    // mozApps to WebIDL in bug 899322.
    Cu.waiveXrays(apps)[i] = createApplicationObject(aWindow, app);
  }

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
        Services.DOMRequest.fireSuccess(req, createApplicationObject(this._window, app));
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
          Services.DOMRequest.fireSuccess(req, createApplicationObject(this._window, app));
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
    this._window.document.nodePrincipal.checkMayLoad(manifestURL, true, false);

    let request = this.createRequest();

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

    if (!this._mgmt)
      this._mgmt = new WebappsApplicationMgmt(this._window);
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
                                         Ci.nsIObserver,
                                         Ci.mozIDOMApplicationRegistry,
                                         Ci.mozIDOMApplicationRegistry2,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{fff440b3-fae2-45c1-bf03-3b5a2e432270}"),
                                    contractID: "@mozilla.org/webapps;1",
                                    interfaces: [Ci.mozIDOMApplicationRegistry,
                                                 Ci.mozIDOMApplicationRegistry2],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Webapps Registry"})
}

/**
  * mozIDOMApplication object
  */

// A simple cache for the wrapped manifests.
let manifestCache = {
  _cache: { },

  // Gets an entry from the cache, and populates the cache if needed.
  get: function mcache_get(aManifestURL, aManifest, aWindow, aInnerWindowID) {
    if (!(aManifestURL in this._cache)) {
      this._cache[aManifestURL] = { };
    }

    let winObjs = this._cache[aManifestURL];
    if (!(aInnerWindowID in winObjs)) {
      winObjs[aInnerWindowID] = Cu.cloneInto(aManifest, aWindow);
    }

    return winObjs[aInnerWindowID];
  },

  // Invalidates an entry in the cache.
  evict: function mcache_evict(aManifestURL, aInnerWindowID) {
    if (aManifestURL in this._cache) {
      let winObjs = this._cache[aManifestURL];
      if (aInnerWindowID in winObjs) {
        delete winObjs[aInnerWindowID];
      }

      if (Object.keys(winObjs).length == 0) {
        delete this._cache[aManifestURL];
      }
    }
  },

  observe: function(aSubject, aTopic, aData) {
    // Clear the cache on memory pressure.
    this._cache = { };
  },

  init: function() {
    Services.obs.addObserver(this, "memory-pressure", false);
  }
};

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
    this._window = aWindow;
    let principal = this._window.document.nodePrincipal;
    this._appStatus = principal.appStatus;
    this.origin = aApp.origin;
    this._manifest = aApp.manifest;
    this._updateManifest = aApp.updateManifest;
    this.manifestURL = aApp.manifestURL;
    this.receipts = aApp.receipts;
    this.installOrigin = aApp.installOrigin;
    this.installTime = aApp.installTime;
    this.installState = aApp.installState || "installed";
    this.removable = aApp.removable;
    this.lastUpdateCheck = aApp.lastUpdateCheck ? aApp.lastUpdateCheck
                                                : Date.now();
    this.updateTime = aApp.updateTime ? aApp.updateTime
                                      : aApp.installTime;
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

    this.initDOMRequestHelper(aWindow, [
      { name: "Webapps:CheckForUpdate:Return:KO", weakRef: true },
      { name: "Webapps:Connect:Return:OK", weakRef: true },
      { name: "Webapps:Connect:Return:KO", weakRef: true },
      { name: "Webapps:FireEvent", weakRef: true },
      { name: "Webapps:GetConnections:Return:OK", weakRef: true },
      { name: "Webapps:UpdateState", weakRef: true }
    ]);

    cpmm.sendAsyncMessage("Webapps:RegisterForMessages", {
      messages: ["Webapps:FireEvent",
                 "Webapps:UpdateState"],
      app: {
        id: this.id,
        manifestURL: this.manifestURL,
        installState: this.installState,
        downloading: this.downloading
      }
    });
  },

  get manifest() {
    return manifestCache.get(this.manifestURL,
                             this._manifest,
                             this._window,
                             this.innerWindowID);
  },

  get updateManifest() {
    return this.updateManifest =
      this._updateManifest ? Cu.cloneInto(this._updateManifest, this._window)
                           : null;
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
    // Only return DOMError when we have an error.
    if (!this._downloadError) {
      return null;
    }
    return new this._window.DOMError(this._downloadError);
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
      browserChild.messageManager.sendAsyncMessage(
        "Webapps:ClearBrowserData",
        { manifestURL: this.manifestURL,
          oid: this._id,
          requestID: this.getRequestId(request) }
      );
    } else {
      Services.DOMRequest.fireErrorAsync(request, "NO_CLEARABLE_BROWSER");
    }
    return request;
  },

  connect: function(aKeyword, aRules) {
    return this.createPromise(function (aResolve, aReject) {
      cpmm.sendAsyncMessage("Webapps:Connect",
                            { keyword: aKeyword,
                              rules: aRules,
                              manifestURL: this.manifestURL,
                              outerWindowID: this._id,
                              requestID: this.getPromiseResolverId({
                                resolve: aResolve,
                                reject: aReject
                              })});
    }.bind(this));
  },

  getConnections: function() {
    return this.createPromise(function (aResolve, aReject) {
      cpmm.sendAsyncMessage("Webapps:GetConnections",
                            { manifestURL: this.manifestURL,
                              outerWindowID: this._id,
                              requestID: this.getPromiseResolverId({
                                resolve: aResolve,
                                reject: aReject
                              })});
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

  uninit: function() {
    this._onprogress = null;
    cpmm.sendAsyncMessage("Webapps:UnregisterForMessages", [
      "Webapps:FireEvent",
      "Webapps:UpdateState"
    ]);

    manifestCache.evict(this.manifestURL, this.innerWindowID);
  },

  _fireEvent: function(aName) {
    let handler = this["_on" + aName];
    if (handler) {
      let event = new this._window.MozApplicationEvent(aName, {
        application: this
      });
      try {
        handler.handleEvent(event);
      } catch (ex) {
        dump("Event handler expection " + ex + "\n");
      }
    }
  },

  _updateState: function(aMsg) {
    if (aMsg.app) {
      for (let prop in aMsg.app) {
        this[prop] = aMsg.app[prop];
      }
    }

    // Intentional use of 'in' so we unset the error if this is explicitly null.
    if ('error' in aMsg) {
      this._downloadError = aMsg.error;
    }

    if (aMsg.manifest) {
      this._manifest = aMsg.manifest;
      manifestCache.evict(this.manifestURL, this.innerWindowID);
    }
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

    // ondownload* callbacks should be triggered on all app instances
    if ((msg.oid != this._id || !req) &&
        aMessage.name !== "Webapps:FireEvent" &&
        aMessage.name !== "Webapps:UpdateState") {
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
      case "Webapps:CheckForUpdate:Return:KO":
        Services.DOMRequest.fireError(req, msg.error);
        break;
      case "Webapps:FireEvent":
        if (msg.manifestURL != this.manifestURL) {
           return;
        }

        // The parent might ask childs to trigger more than one event in one
        // shot, so in order to avoid needless IPC we allow an array for the
        // 'eventType' IPC message field.
        if (!Array.isArray(msg.eventType)) {
          msg.eventType = [msg.eventType];
        }

        msg.eventType.forEach((aEventType) => {
          // If we are in a successful state clear any past errors.
          if (aEventType === 'downloadapplied' ||
              aEventType === 'downloadsuccess') {
            this._downloadError = null;
          }

          if ("_on" + aEventType in this) {
            this._fireEvent(aEventType);
          } else {
            dump("Unsupported event type " + aEventType + "\n");
          }
        });

        if (req) {
          Services.DOMRequest.fireSuccess(req, this.manifestURL);
        }
        break;
      case "Webapps:UpdateState":
        if (msg.manifestURL != this.manifestURL) {
          return;
        }

        this._updateState(msg);
        break;
      case "Webapps:ClearBrowserData:Return":
        this.removeMessageListeners(aMessage.name);
        Services.DOMRequest.fireSuccess(req, null);
        break;
      case "Webapps:Connect:Return:OK":
        let messagePorts = [];
        msg.messagePortIDs.forEach((aPortID) => {
          let port = new this._window.MozInterAppMessagePort(aPortID);
          messagePorts.push(port);
        });
        req.resolve(messagePorts);
        break;
      case "Webapps:Connect:Return:KO":
        req.reject("No connections registered");
        break;
      case "Webapps:GetConnections:Return:OK":
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
        this.receipts = msg.receipts;
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
        this.receipts = msg.receipts;
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
        this.receipts = msg.receipts;
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

  QueryInterface: XPCOMUtils.generateQI([Ci.mozIDOMApplication,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

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
  this.initDOMRequestHelper(aWindow, ["Webapps:GetAll:Return:OK",
                                      "Webapps:GetAll:Return:KO",
                                      "Webapps:Uninstall:Return:OK",
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
    dump("-- webapps.js uninstall " + aApp.manifestURL + "\n");
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:Uninstall", { origin: aApp.origin,
                                                 manifestURL: aApp.manifestURL,
                                                 oid: this._id,
                                                 requestID: this.getRequestId(request) });
    return request;
  },

  getAll: function() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("Webapps:GetAll", { oid: this._id,
                                              requestID: this.getRequestId(request) });
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
    this._oninstall = aCallback;
  },

  set onuninstall(aCallback) {
    this._onuninstall = aCallback;
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
      case "Webapps:Uninstall:Broadcast:Return:OK":
        if (this._onuninstall) {
          let detail = {
            manifestURL: msg.manifestURL,
            origin: msg.origin
          };
          let event = new this._window.MozApplicationEvent("applicationuninstall",
                           { application : createApplicationObject(this._window, detail) });
          this._onuninstall.handleEvent(event);
        }
        break;
      case "Webapps:Uninstall:Return:OK":
        Services.DOMRequest.fireSuccess(req, msg.origin);
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

  QueryInterface: XPCOMUtils.generateQI([Ci.mozIDOMApplicationMgmt,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{8c1bca96-266f-493a-8d57-ec7a95098c15}"),
                                    contractID: "@mozilla.org/webapps/application-mgmt;1",
                                    interfaces: [Ci.mozIDOMApplicationMgmt],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Webapps Application Mgmt"})
}

manifestCache.init();

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WebappsRegistry,
                                                     WebappsApplication]);
