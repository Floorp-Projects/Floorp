/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = ["AndroidUtils"];

Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Messaging",
                                  "resource://gre/modules/Messaging.jsm");

var appsRegistry = null;

function debug() {
  //dump("-*- AndroidUtils " + Array.slice(arguments) + "\n");
}

// Helper functions to manage Android native apps. We keep them in the
// registry with a `kind` equals to "android-native" and we also store
// the package name and class name in the registry.
// Communication with the android side happens through json messages.

this.AndroidUtils = {
  init: function(aRegistry) {
    appsRegistry = aRegistry;
    Services.obs.addObserver(this, "Android:Apps:Installed", false);
    Services.obs.addObserver(this, "Android:Apps:Uninstalled", false);
  },

  uninit: function() {
    Services.obs.removeObserver(this, "Android:Apps:Installed");
    Services.obs.removeObserver(this, "Android:Apps:Uninstalled");
  },

  getOriginAndManifestURL: function(aPackageName) {
    let origin = "android://" + aPackageName.toLowerCase();
    let manifestURL = origin + "/manifest.webapp";
    return [origin, manifestURL];
  },

  getPackageAndClassFromManifestURL: function(aManifestURL) {
    debug("getPackageAndClassFromManifestURL " + aManifestURL);
    let app = appsRegistry.getAppByManifestURL(aManifestURL);
    if (!app) {
      debug("No app for " + aManifestURL);
      return [];
    }
    return [app.android_packagename, app.android_classname];
  },

  buildAndroidAppData: function(aApp) {
    // Use the package and class name to get a unique origin.
    // We put the version with the normal case as part of the manifest url.
    let [origin, manifestURL] =
      this.getOriginAndManifestURL(aApp.packagename);
    // We choose 96 as an arbitrary size since we can only get one icon
    // from Android.
    let manifest = {
      name: aApp.name,
      icons: { "96": aApp.icon }
    }
    debug("Origin is " + origin);
    let appData = {
      app: {
        installOrigin: origin,
        origin: origin,
        manifest: manifest,
        manifestURL: manifestURL,
        manifestHash: AppsUtils.computeHash(JSON.stringify(manifest)),
        appStatus: Ci.nsIPrincipal.APP_STATUS_INSTALLED,
        removable: aApp.removable,
        android_packagename: aApp.packagename,
        android_classname: aApp.classname
      },
      isBrowser: false,
      isPackage: false
    };

    return appData;
  },

  installAndroidApps: function() {
    return Messaging.sendRequestForResult({ type: "Apps:GetList" }).then(
      aApps => {
        debug("Got " + aApps.apps.length + " android apps.");
        let promises = [];
        aApps.apps.forEach(app => {
          debug("App is " + app.name + " removable? " + app.removable);
          let p = new Promise((aResolveInstall, aRejectInstall) => {
            let appData = this.buildAndroidAppData(app);
            appsRegistry.confirmInstall(appData, null, aResolveInstall);
          });
          promises.push(p);
        });

        // Wait for all apps to be installed.
        return Promise.all(promises);
      }
    ).then(appsRegistry._saveApps.bind(appsRegistry));
  },

  observe: function(aSubject, aTopic, aData) {
    let data;
    try {
      data = JSON.parse(aData);
    } catch(e) {
      debug(e);
      return;
    }

    if (aTopic == "Android:Apps:Installed") {
      let appData = this.buildAndroidAppData(data);
      appsRegistry.confirmInstall(appData);
    } else if (aTopic == "Android:Apps:Uninstalled") {
      let [origin, manifestURL] =
        this.getOriginAndManifestURL(data.packagename);
      appsRegistry.uninstall(manifestURL);
    }
  },
}
