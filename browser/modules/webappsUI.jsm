/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = ["webappsUI"];

let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource:///modules/WebappsInstaller.jsm");
Cu.import("resource://gre/modules/WebappOSUtils.jsm");

let webappsUI = {
  init: function webappsUI_init() {
    Services.obs.addObserver(this, "webapps-ask-install", false);
    Services.obs.addObserver(this, "webapps-launch", false);
  },
  
  uninit: function webappsUI_uninit() {
    Services.obs.removeObserver(this, "webapps-ask-install");
    Services.obs.removeObserver(this, "webapps-launch");
  },

  observe: function webappsUI_observe(aSubject, aTopic, aData) {
    let data = JSON.parse(aData);

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
    }
  },

  openURL: function(aUrl, aOrigin) {
    let browserEnumerator = Services.wm.getEnumerator("navigator:browser");  
    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

    // Check each browser instance for our URL
    let found = false;
    while (!found && browserEnumerator.hasMoreElements()) {
      let browserWin = browserEnumerator.getNext();
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

    let chromeWin = browser.ownerDocument.defaultView;
    let bundle = chromeWin.gNavigatorBundle;

    let mainAction = {
      label: bundle.getString("webapps.install"),
      accessKey: bundle.getString("webapps.install.accesskey"),
      callback: function() {
        let app = WebappsInstaller.install(aData);
        if (app) {
          let localDir = null;
          if (app.appcacheDefined && app.appProfile) {
            localDir = app.appProfile.localDir;
          }

          DOMApplicationRegistry.confirmInstall(aData, false, localDir);
          installationSuccessNotification(app, chromeWin);
        } else {
          DOMApplicationRegistry.denyInstall(aData);
        }
      }
    };

    let requestingURI = chromeWin.makeURI(aData.from);
    let manifest = new DOMApplicationManifest(aData.app.manifest, aData.app.origin);

    let host;
    try {
      host = requestingURI.host;
    } catch(e) {
      host = requestingURI.spec;
    }

    let message = bundle.getFormattedString("webapps.requestInstall",
                                            [manifest.name, host]);

    chromeWin.PopupNotifications.show(browser, "webapps-install", message,
                                      "webapps-notification-icon", mainAction);
  },

  _getWindowForId: function(aId) {
     let someWindow = Services.wm.getMostRecentWindow(null);
     return someWindow &&
            someWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils)
                      .getOuterWindowWithId(aId);
  }
}

function installationSuccessNotification(app, aWindow) {
  let bundle = aWindow.gNavigatorBundle;

  if (("@mozilla.org/alerts-service;1" in Cc)) {
    let notifier;
    try {
      notifier = Cc["@mozilla.org/alerts-service;1"].
                 getService(Ci.nsIAlertsService);

      notifier.showAlertNotification(app.iconURI.spec,
                                    bundle.getString("webapps.install.success"),
                                    app.appNameAsFilename,
                                    false, null, null);

    } catch (ex) {}
  }
}
