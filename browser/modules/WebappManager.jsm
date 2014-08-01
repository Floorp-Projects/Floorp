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
    Services.obs.addObserver(this, "webapps-launch", false);
    Services.obs.addObserver(this, "webapps-uninstall", false);
    cpmm.addMessageListener("Webapps:Install:Return:OK", this);
    cpmm.addMessageListener("Webapps:Install:Return:KO", this);
    cpmm.addMessageListener("Webapps:UpdateState", this);
  },

  uninit: function() {
    Services.obs.removeObserver(this, "webapps-ask-install");
    Services.obs.removeObserver(this, "webapps-launch");
    Services.obs.removeObserver(this, "webapps-uninstall");
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

    switch(aTopic) {
      case "webapps-ask-install":
        let win = this._getWindowForId(data.oid);
        if (win && win.location.href == data.from) {
          this.doInstall(data, win);
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

  _getWindowForId: function(aId) {
    let someWindow = Services.wm.getMostRecentWindow(null);
    return someWindow && Services.wm.getOuterWindowWithId(aId);
  },

  doInstall: function(aData, aWindow) {
    let browser = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell)
                         .chromeEventHandler;
    let chromeDoc = browser.ownerDocument;
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
                        show(browser,
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
          notifyInstallSuccess(aData.app, nativeApp, bundle);
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

    let host;
    try {
      host = requestingURI.host;
    } catch(e) {
      host = requestingURI.spec;
    }

    let message = bundle.getFormattedString("webapps.requestInstall",
                                            [manifest.name, host], 2);

    notification = chromeWin.PopupNotifications.show(browser,
                                                     "webapps-install",
                                                     message,
                                                     "webapps-notification-icon",
                                                     mainAction);

  }
}

function notifyInstallSuccess(aApp, aNativeApp, aBundle) {
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
                                   true, null, launcher);
  } catch (ex) {}
}
