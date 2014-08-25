/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

function debug(s) {
  dump("-*- B2GAppMigrator.js: " + s + "\n");
}
const DEBUG = false;

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const kMigrationMessageName = "webapps-before-update-merge";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

function B2GAppMigrator() {
  Services.obs.addObserver(this, kMigrationMessageName, false);
  Services.obs.addObserver(this, "xpcom-shutdown", false);
}

B2GAppMigrator.prototype = {
  classID:         Components.ID('{7211ece0-b458-4635-9afc-f8d7f376ee95}'),
  QueryInterface:  XPCOMUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),
  executeBrowserMigration: function() {
    // The browser db name is hashed the same way everywhere, so it
    // should be the same on all systems. We should be able to just
    // hardcode it.
    let browserDBFileName = "2959517650brreosw.sqlite";

    // Storage directories need to be prefixed with the local id of
    // the app
    let browserLocalAppId = appsService.getAppLocalIdByManifestURL("app://browser.gaiamobile.org/manifest.webapp");
    let browserAppStorageDirName = browserLocalAppId + "+f+app+++browser.gaiamobile.org";
    let browserDBFile = FileUtils.getFile("ProfD",
                                          ["storage",
                                           "persistent",
                                           browserAppStorageDirName,
                                           "idb",
                                           browserDBFileName], true);

    if (!browserDBFile.exists()) {
      if (DEBUG) debug("Browser DB file does not exist, nothing to copy");
      return;
    }

    let systemLocalAppId = appsService.getAppLocalIdByManifestURL("app://system.gaiamobile.org/manifest.webapp");
    let systemAppStorageDirName = systemLocalAppId + "+f+app+++system.gaiamobile.org";
    let systemDBDir = FileUtils.getDir("ProfD",
                                       ["storage",
                                        "persistent",
                                        systemAppStorageDirName,
                                        "idb"], false, true);

    if (DEBUG) {
      debug("Browser DB file exists, copying");
      debug("Browser local id: " + browserLocalAppId + "");
      debug("System local id: " + systemLocalAppId + "");
      debug("Browser DB file path: " + browserDBFile.path + "");
      debug("System DB directory path: " + systemDBDir.path + "");
    }

    try {
      browserDBFile.copyTo(systemDBDir, browserDBFileName);
    } catch (e) {
      debug("File copy caused error! " + e.name);
    }
    if (DEBUG) debug("Browser DB copied successfully");
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case kMigrationMessageName:
        this.executeBrowserMigration();
        Services.obs.removeObserver(this, kMigrationMessageName);
        break;
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, kMigrationMessageName);
        Services.obs.removeObserver(this, "xpcom-shutdown");
        break;
      default:
        debug("Unhandled topic: " + topic);
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([B2GAppMigrator]);
