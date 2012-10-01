/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const XRE_OS_UPDATE_APPLY_TO_DIR = "OSUpdApplyToD"
const LOCAL_DIR = "/data/local";

XPCOMUtils.defineLazyServiceGetter(Services, "env",
                                   "@mozilla.org/process/environment;1",
                                   "nsIEnvironment");

function DirectoryProvider() {
}

DirectoryProvider.prototype = {
  classID: Components.ID("{9181eb7c-6f87-11e1-90b1-4f59d80dd2e5}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider]),

  getFile: function dp_getFile(prop, persistent) {
#ifdef MOZ_WIDGET_GONK
    let localProps = ["cachePDir", "webappsDir", "PrefD", "indexedDBPDir",
                      "permissionDBPDir", "UpdRootD"];
    if (localProps.indexOf(prop) != -1) {
      let file = Cc["@mozilla.org/file/local;1"]
                   .createInstance(Ci.nsILocalFile)
      file.initWithPath(LOCAL_DIR);
      persistent.value = true;
      return file;
    } else if (prop == "coreAppsDir") {
      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile)
      file.initWithPath("/system/b2g");
      persistent.value = true;
      return file;
    } else if (prop == XRE_OS_UPDATE_APPLY_TO_DIR) {
      return this.getOSUpdateApplyToDir(persistent);
    }
#endif

    return null;
  },

  getOSUpdateApplyToDir: function dp_getOSUpdateApplyToDir(persistent) {
    // TODO add logic to check available storage space,
    // and iterate through pref(s) to find alternative dirs if
    // necessary.

    let path = Services.env.get("EXTERNAL_STORAGE");
    if (!path) {
      path = LOCAL_PATH;
    }

    let dir = Cc["@mozilla.org/file/local;1"]
                 .createInstance(Ci.nsILocalFile)
    dir.initWithPath(path);

    if (!dir.exists() && path != LOCAL_PATH) {
      // Fallback to LOCAL_PATH if we didn't fallback earlier
      dir.initWithPath(LOCAL_PATH);

      if (!dir.exists()) {
        throw Cr.NS_ERROR_FILE_NOT_FOUND;
      }
    }

    dir.appendRelativePath("updates");
    persistent.value = false;
    return dir;
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([DirectoryProvider]);
