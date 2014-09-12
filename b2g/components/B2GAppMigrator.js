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

const kIDBDirType = "indexedDBPDir";
const kProfileDirType = "ProfD";

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

    // On the phone, the browser db will only be in the old IDB
    // directory, since it only existed up until v2.0. On desktop, it
    // will exist in the profile directory.
    //
    // Uses getDir with filename appending to make sure we don't
    // create extra directories along the way if they don't already
    // exist.
    let browserDBFile = FileUtils.getDir(kIDBDirType,
                                         ["storage",
                                          "persistent",
                                          browserAppStorageDirName,
                                          "idb"], false, true);
    browserDBFile.append(browserDBFileName);

    if (!browserDBFile.exists()) {
      if (DEBUG) debug("Browser DB directory " + browserDBFile.path + " does not exist, trying profile location");
      browserDBFile = FileUtils.getDir(kProfileDirType,
                                        ["storage",
                                         "persistent",
                                         browserAppStorageDirName,
                                         "idb"], false, true);
      browserDBFile.append(browserDBFileName);
      if (!browserDBFile.exists()) {
        if (DEBUG) debug("Browser DB directory " + browserDBFile.path + " does not exist. Cannot copy browser db.");
        return;
      }
    }

    let systemLocalAppId = appsService.getAppLocalIdByManifestURL("app://system.gaiamobile.org/manifest.webapp");
    let systemAppStorageDirName = systemLocalAppId + "+f+app+++system.gaiamobile.org";

    // This check futureproofs the system DB storage directory. It
    // currently exists outside of the profile but will most likely
    // move into the profile at some point.
    let systemDBDir = FileUtils.getDir(kIDBDirType,
                                       ["storage",
                                        "persistent",
                                        systemAppStorageDirName,
                                        "idb"], false, true);

    if (!systemDBDir.exists()) {
      if (DEBUG) debug("System DB directory " + systemDBDir.path + " does not exist, trying profile location");
      systemDBDir = FileUtils.getDir(kProfileDirType,
                                     ["storage",
                                      "persistent",
                                      systemAppStorageDirName,
                                      "idb"], false, true);
      if (!systemDBDir.exists()) {
        if (DEBUG) debug("System DB directory " + systemDBDir.path + " does not exist. Cannot copy browser db.");
        return;
      }
    }

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
