/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const LOCAL_DIR = "/data/local";

function DirectoryProvider() {
}

DirectoryProvider.prototype = {
  classID: Components.ID("{9181eb7c-6f87-11e1-90b1-4f59d80dd2e5}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider]),

  getFile: function dp_getFile(prop, persistent) {
#ifdef MOZ_WIDGET_GONK
    let localProps = ["cachePDir", "webappsDir", "PrefD", "indexedDBPDir"];
    if (localProps.indexOf(prop) != -1) {
      prop.persistent = true;
      let file = Cc["@mozilla.org/file/local;1"]
                   .createInstance(Ci.nsILocalFile)
      file.initWithPath(LOCAL_DIR);
      return file;
    }
#endif

    return null;
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([DirectoryProvider]);
