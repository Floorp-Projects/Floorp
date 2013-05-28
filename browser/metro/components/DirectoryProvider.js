/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// -----------------------------------------------------------------------
// Directory Provider for special browser folders and files
// -----------------------------------------------------------------------

const NS_APP_CACHE_PARENT_DIR = "cachePDir";

function DirectoryProvider() {}

DirectoryProvider.prototype = {
  classID: Components.ID("{ef0f7a87-c1ee-45a8-8d67-26f586e46a4b}"),
  
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider]),

  getFile: function(prop, persistent) {
    if (prop == NS_APP_CACHE_PARENT_DIR) {
      let dirsvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
      let profile = dirsvc.get("ProfD", Ci.nsIFile);

      let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
      let device = sysInfo.get("device");
      switch (device) {
        default:
          return profile;
      }
    }

    // We are retuning null to show failure instead for throwing an error. The
    // interface is called quite a bit and throwing an error is noisy. Returning
    // null works with the way the interface is called [see bug 529077]
    return null;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DirectoryProvider]);

