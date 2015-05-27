/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["WebappManager"];

let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NativeApp",
  "resource://gre/modules/NativeApp.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebappOSUtils",
  "resource://gre/modules/WebappOSUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

this.WebappManager = {
  // List of promises for in-progress installations
  installations: {},

  init: function() {
    Services.obs.addObserver(this, "webapps-ask-install", false);
    Services.obs.addObserver(this, "webapps-ask-uninstall", false);
    Services.obs.addObserver(this, "webapps-launch", false);
    Services.obs.addObserver(this, "webapps-uninstall", false);
    cpmm.sendAsyncMessage("Webapps:RegisterForMessages",
                          { messages: ["Webapps:Install:Return:OK",
                                       "Webapps:Install:Return:KO",
                                       "Webapps:UpdateState"]});
    cpmm.addMessageListener("Webapps:Install:Return:OK", this);
    cpmm.addMessageListener("Webapps:Install:Return:KO", this);
    cpmm.addMessageListener("Webapps:UpdateState", this);
  },

  uninit: function() {
    Services.obs.removeObserver(this, "webapps-ask-install");
    Services.obs.removeObserver(this, "webapps-ask-uninstall");
    Services.obs.removeObserver(this, "webapps-launch");
    Services.obs.removeObserver(this, "webapps-uninstall");
    cpmm.sendAsyncMessage("Webapps:UnregisterForMessages",
                          ["Webapps:Install:Return:OK",
                           "Webapps:Install:Return:KO",
                           "Webapps:UpdateState"]);
    cpmm.removeMessageListener("Webapps:Install:Return:OK", this);
    cpmm.removeMessageListener("Webapps:Install:Return:KO", this);
    cpmm.removeMessageListener("Webapps:UpdateState", this);
  },

  receiveMessage: function(aMessage) {
    let data = aMessage.data;

    let manifestURL = data.manifestURL ||
                      (data.app && data.app.manifestURL) ||
                      data.manifest;

    if (!this.installations[manifestURL]) {
      return;
    }

    if (aMessage.name == "Webapps:UpdateState") {
      if (data.error) {
        this.installations[manifestURL].reject(data.error);
      } else if (data.app.installState == "installed") {
        this.installations[manifestURL].resolve();
      }
    } else if (aMessage.name == "Webapps:Install:Return:OK" &&
               !data.isPackage) {
      let manifest = new ManifestHelper(data.app.manifest,
                                        data.app.origin,
                                        data.app.manifestURL);
      if (!manifest.appcache_path) {
        this.installations[manifestURL].resolve();
      }
    } else if (aMessage.name == "Webapps:Install:Return:KO") {
      this.installations[manifestURL].reject(data.error);
    }
  },

  observe: function(aSubject, aTopic, aData) {
    let data = JSON.parse(aData);
    data.mm = aSubject;

    let browser;
    switch(aTopic) {
      case "webapps-ask-install":
        browser = this._getBrowserForId(data.topId);
        if (browser) {
          this.doInstall(data, browser);
        }
        break;
      case "webapps-ask-uninstall":
        browser = this._getBrowserForId(data.topId);
        if (browser) {
          this.doUninstall(data, browser);
        }
        break;
      case "webapps-launch":
        WebappOSUtils.launch(data);
        break;
      case "webapps-uninstall":
        WebappOSUtils.uninstall(data);
        break;
    }
  },

  _getBrowserForId: function(aId) {
    let windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      let window = windows.getNext();
      let tabbrowser = window.gBrowser;
      let foundBrowser = tabbrowser.getBrowserForOuterWindowID(aId);
      if (foundBrowser) {
        return foundBrowser;
      }
    }
    let foundWindow = Services.wm.getOuterWindowWithId(aId);
    if (foundWindow) {
      return foundWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell)
                        .chromeEventHandler;
    }
    return null;
  },

  doInstall: function(aData, aBrowser) {
    let chromeDoc = aBrowser.ownerDocument;
    let chromeWin = chromeDoc.defaultView;
    let popupProgressContent =
      chromeDoc.getElementById("webapps-install-progress-content");

    let bundle = chromeWin.gNavigatorBundle;

    let jsonManifest = aData.isPackage ? aData.app.updateManifest : aData.app.manifest;

    let notification;

    let mainAction = {
      label: bundle.getString("webapps.install"),
      accessKey: bundle.getString("webapps.install.accesskey"),
      callback: () => {
        notification.remove();

        notification = chromeWin.PopupNotifications.
                        show(aBrowser,
                             "webapps-install-progress",
                             bundle.getString("webapps.install.inprogress"),
                             "webapps-notification-icon");

        let progressMeter = chromeDoc.createElement("progressmeter");
        progressMeter.setAttribute("mode", "undetermined");
        popupProgressContent.appendChild(progressMeter);

        let manifestURL = aData.app.manifestURL;

        let nativeApp = new NativeApp(aData.app, jsonManifest,
                                      aData.app.categories);

        this.installations[manifestURL] = Promise.defer();
        this.installations[manifestURL].promise.then(() => {
          notifyInstallSuccess(aData.app, nativeApp, bundle,
                               PrivateBrowsingUtils.isBrowserPrivate(aBrowser));
        }, (error) => {
          Cu.reportError("Error installing webapp: " + error);
        }).then(() => {
          popupProgressContent.removeChild(progressMeter);
          delete this.installations[manifestURL];
          if (Object.getOwnPropertyNames(this.installations).length == 0) {
            notification.remove();
          }
        });

        let localDir;
        try {
          localDir = nativeApp.createProfile();
        } catch (ex) {
          DOMApplicationRegistry.denyInstall(aData);
          return;
        }

        DOMApplicationRegistry.confirmInstall(aData, localDir,
          Task.async(function*(aApp, aManifest, aZipPath) {
            try {
              yield nativeApp.install(aApp, aManifest, aZipPath);
            } catch (ex) {
              Cu.reportError("Error installing webapp: " + ex);
              throw ex;
            }
          })
        );
      }
    };

    let requestingURI = chromeWin.makeURI(aData.from);
    let app = aData.app;
    let manifest = new ManifestHelper(jsonManifest, app.origin, app.manifestURL);

    let options = {};
    try {
      options.displayOrigin = requestingURI.host;
    } catch(e) {
      options.displayOrigin = requestingURI.spec;
    }

    let message = bundle.getFormattedString("webapps.requestInstall2",
                                            [manifest.name]);

    let gBrowser = chromeWin.gBrowser;
    if (gBrowser) {
      let windowID = aData.oid;

      let listener = {
        onLocationChange(webProgress) {
          if (webProgress.DOMWindowID == windowID) {
            notification.remove();
          }
        }
      };

      gBrowser.addProgressListener(listener);

      options.eventCallback = event => {
        if (event != "removed") {
          return;
        }
        // The notification was removed, so we should
        // remove our listener.
        gBrowser.removeProgressListener(listener);
      };
    }

    notification = chromeWin.PopupNotifications.show(aBrowser,
                                                     "webapps-install",
                                                     message,
                                                     "webapps-notification-icon",
                                                     mainAction, [],
                                                     options);
  },

  doUninstall: function(aData, aBrowser) {
    let chromeDoc = aBrowser.ownerDocument;
    let chromeWin = chromeDoc.defaultView;

    let bundle = chromeWin.gNavigatorBundle;
    let jsonManifest = aData.app.manifest;

    let notification;

    let mainAction = {
      label: bundle.getString("webapps.uninstall"),
      accessKey: bundle.getString("webapps.uninstall.accesskey"),
      callback: () => {
        notification.remove();
        DOMApplicationRegistry.confirmUninstall(aData);
      }
    };

    let secondaryAction = {
      label: bundle.getString("webapps.doNotUninstall"),
      accessKey: bundle.getString("webapps.doNotUninstall.accesskey"),
      callback: () => {
        notification.remove();
        DOMApplicationRegistry.denyUninstall(aData, "USER_DECLINED");
      }
    };

    let manifest = new ManifestHelper(jsonManifest, aData.app.origin,
                                      aData.app.manifestURL);

    let message = bundle.getFormattedString("webapps.requestUninstall",
                                            [manifest.name]);


    let options = {};
    let gBrowser = chromeWin.gBrowser;
    if (gBrowser) {
      let windowID = aData.oid;

      let listener = {
        onLocationChange(webProgress) {
          if (webProgress.DOMWindowID == windowID) {
            notification.remove();
          }
        }
      };

      gBrowser.addProgressListener(listener);

      options.eventCallback = event => {
        if (event != "removed") {
          return;
        }
        // The notification was removed, so we should
        // remove our listener.
        gBrowser.removeProgressListener(listener);
      };
    }

    notification = chromeWin.PopupNotifications.show(
                     aBrowser, "webapps-uninstall", message,
                     "webapps-notification-icon",
                     mainAction, [secondaryAction],
                     options);
  }
}

function notifyInstallSuccess(aApp, aNativeApp, aBundle, aInPrivateBrowsing) {
  let launcher = {
    observe: function(aSubject, aTopic) {
      if (aTopic == "alertclickcallback") {
        WebappOSUtils.launch(aApp);
      }
    }
  };

  try {
    let notifier = Cc["@mozilla.org/alerts-service;1"].
                   getService(Ci.nsIAlertsService);

    notifier.showAlertNotification(aNativeApp.iconURI.spec,
                                   aBundle.getString("webapps.install.success"),
                                   aNativeApp.appNameAsFilename,
                                   true, null, launcher, "", "", "", "", null,
                                   aInPrivateBrowsing);
  } catch (ex) {}
}
