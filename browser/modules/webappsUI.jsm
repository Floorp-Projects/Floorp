/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["webappsUI"];

let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/WebappsInstaller.jsm");
Cu.import("resource://gre/modules/WebappOSUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

this.webappsUI = {
  // List of promises for in-progress installations
  installations: {},

  init: function webappsUI_init() {
    Services.obs.addObserver(this, "webapps-ask-install", false);
    Services.obs.addObserver(this, "webapps-launch", false);
    Services.obs.addObserver(this, "webapps-uninstall", false);
    cpmm.addMessageListener("Webapps:Install:Return:OK", this);
    cpmm.addMessageListener("Webapps:Install:Return:KO", this);
    cpmm.addMessageListener("Webapps:UpdateState", this);
  },

  uninit: function webappsUI_uninit() {
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
      let manifest = new ManifestHelper(data.app.manifest, data.app.origin);
      if (!manifest.appcache_path) {
        this.installations[manifestURL].resolve();
      }
    } else if (aMessage.name == "Webapps:Install:Return:KO") {
      this.installations[manifestURL].reject(data.error);
    }
  },

  observe: function webappsUI_observe(aSubject, aTopic, aData) {
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

  openURL: function(aUrl, aOrigin) {
    let browserEnumerator = Services.wm.getEnumerator("navigator:browser");
    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

    // Check each browser instance for our URL
    let found = false;
    while (!found && browserEnumerator.hasMoreElements()) {
      let browserWin = browserEnumerator.getNext();
      if (browserWin.closed) {
        continue;
      }
      let tabbrowser = browserWin.gBrowser;

      // Check each tab of this browser instance
      let numTabs = tabbrowser.tabs.length;
      for (let index = 0; index < numTabs; index++) {
        let tab = tabbrowser.tabs[index];
        let appURL = ss.getTabValue(tab, "appOrigin");
        if (appURL == aOrigin) {
          // The URL is already opened. Select this tab.
          tabbrowser.selectedTab = tab;
          browserWin.focus();
          found = true;
          break;
        }
      }
    }

    // Our URL isn't open. Open it now.
    if (!found) {
      let recentWindow = Services.wm.getMostRecentWindow("navigator:browser");
      if (recentWindow) {
        // Use an existing browser window
        let browser = recentWindow.gBrowser;
        let tab = browser.addTab(aUrl);
        browser.pinTab(tab);
        browser.selectedTab = tab;
        ss.setTabValue(tab, "appOrigin", aOrigin);
      }
    }
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

        let cleanup = (ex) => {
          popupProgressContent.removeChild(progressMeter);
          delete this.installations[manifestURL];
          if (Object.getOwnPropertyNames(this.installations).length == 0) {
            notification.remove();
          }
        };

        this.installations[manifestURL] = Promise.defer();
        this.installations[manifestURL].promise.then(null, (error) => {
          Cu.reportError("Error installing webapp: " + error);
          cleanup();
        });

        let app = WebappsInstaller.init(aData);

        if (app) {
          let localDir = null;
          if (app.appProfile) {
            localDir = app.appProfile.localDir;
          }

          DOMApplicationRegistry.confirmInstall(aData, localDir,
            (aManifest, aZipPath) => {
              Task.spawn(function() {
                try {
                  yield WebappsInstaller.install(aData, aManifest, aZipPath);
                  yield this.installations[manifestURL].promise;
                  installationSuccessNotification(aData, app, bundle);
                } catch (ex) {
                  Cu.reportError("Error installing webapp: " + ex);
                  // TODO: Notify user that the installation has failed
                } finally {
                  cleanup();
                }
              }.bind(this));
            });
        } else {
          DOMApplicationRegistry.denyInstall(aData);
          cleanup();
        }
      }
    };

    let requestingURI = chromeWin.makeURI(aData.from);
    let jsonManifest = aData.isPackage ? aData.app.updateManifest : aData.app.manifest;
    let manifest = new ManifestHelper(jsonManifest, aData.app.origin);

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

function installationSuccessNotification(aData, app, aBundle) {
  let launcher = {
    observe: function(aSubject, aTopic) {
      if (aTopic == "alertclickcallback") {
        WebappOSUtils.launch(aData.app);
      }
    }
  };

  try {
    let notifier = Cc["@mozilla.org/alerts-service;1"].
                   getService(Ci.nsIAlertsService);

    notifier.showAlertNotification(app.iconURI.spec,
                                   aBundle.getString("webapps.install.success"),
                                   app.appNameAsFilename,
                                   true, null, launcher);
  } catch (ex) {}
}
