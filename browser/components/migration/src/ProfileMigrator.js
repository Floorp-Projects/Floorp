/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

function ProfileMigrator() {
}

ProfileMigrator.prototype = {
  migrate: function PM_migrate(aStartup, aKey) {
    // By opening the wizard with a supplied migrator, it will automatically
    // migrate from it.
    let key = null, migrator = null;
    let skipImportSourcePage = Cc["@mozilla.org/supports-PRBool;1"]
                                 .createInstance(Ci.nsISupportsPRBool);
    if (aKey) {
      key = aKey;
      migrator = this._getMigratorIfSourceExists(key);
      if (!migrator) {
        Cu.reportError("Invalid migrator key specified or source does not exist.");
        return;
      }
      // If the migrator was passed to us from the caller, use that migrator
      // and skip the import source page.
      skipImportSourcePage.data = true;
    } else {
      [key, migrator] = this._getDefaultMigrator();
    }
    if (!key)
        return;

    let params = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    params.appendElement(this._toCString(key), false);
    params.appendElement(migrator, false);
    params.appendElement(aStartup, false);
    params.appendElement(skipImportSourcePage, false);

    Services.ww.openWindow(null,
                           "chrome://browser/content/migration/migration.xul",
                           "_blank",
                           "chrome,dialog,modal,centerscreen,titlebar",
                           params);
  },

  _toCString: function PM__toCString(aStr) {
    let cstr = Cc["@mozilla.org/supports-cstring;1"].
               createInstance(Ci.nsISupportsCString);
    cstr.data = aStr;
    return cstr;
  },

  _getMigratorIfSourceExists: function PM__getMigratorIfSourceExists(aKey) {
    try {
      let cid = "@mozilla.org/profile/migrator;1?app=browser&type=" + aKey;
      let migrator = Cc[cid].createInstance(Ci.nsIBrowserProfileMigrator);
      if (migrator.sourceExists)
        return migrator;
    } catch (ex) {
      Cu.reportError("Could not get migrator: " + ex);
    }
    return null;
  },

  // We don't yet support checking for the default browser on all platforms,
  // needless to say we don't have migrators for all browsers.  Thus, for each
  // platform, there's a fallback list of migrators used in these cases.
  _PLATFORM_FALLBACK_LIST:
#ifdef XP_WIN
     ["ie", "chrome", /* safari */],
#elifdef XP_MACOSX
     ["safari", "chrome"],
#else
     ["chrome"],
#endif

  _getDefaultMigrator: function PM__getDefaultMigrator() {
    let migratorsOrdered = Array.slice(this._PLATFORM_FALLBACK_LIST);
    let defaultBrowser = "";
#ifdef XP_WIN
    try {
      const REG_KEY = "SOFTWARE\\Classes\\HTTP\\shell\\open\\command";
      let regKey = Cc["@mozilla.org/windows-registry-key;1"].
                   createInstance(Ci.nsIWindowsRegKey);
      regKey.open(regKey.ROOT_KEY_LOCAL_MACHINE, REG_KEY,
                  regKey.ACCESS_READ);
      let value = regKey.readStringValue("").toLowerCase();      
      let pathMatches = value.match(/^"?(.+?\.exe)"?/);
      if (!pathMatches) {
        throw new Error("Could not extract path from " +
                        REG_KEY + "(" + value + ")");
      }
 
      // We want to find out what the default browser is but the path in and of
      // itself isn't enough.  Why? Because sometimes on Windows paths get
      // truncated like so: C:\PROGRA~1\MOZILL~2\MOZILL~1.EXE.  How do we know
      // what product that is? Mozilla's file objects do nothing to 'normalize'
      // the path so we need to attain an actual product descriptor from the
      // file somehow, and in this case it means getting the "InternalName"
      // field of the file's VERSIONINFO resource. 
      //
      // In the file's resource segment there is a VERSIONINFO section that is
      // laid out like this:
      //
      // VERSIONINFO
      //   StringFileInfo
      //     <TranslationID>
      //       InternalName           "iexplore"
      //   VarFileInfo
      //     Translation              <TranslationID>
      //
      // By Querying the VERSIONINFO section for its Tranlations, we can find
      // out where the InternalName lives (A file can have more than one
      // translation of its VERSIONINFO segment, but we just assume the first
      // one).
      let file = FileUtils.File(pathMatches[1])
                          .QueryInterface(Ci.nsILocalFileWin);
      switch (file.getVersionInfoField("InternalName").toLowerCase()) {
        case "iexplore":
          defaultBrowser = "ie";
          break;
        case "chrome":
          defaultBrowser = "chrome";
          break;
      }
    }
    catch (ex) {
      Cu.reportError("Could not retrieve default browser: " + ex);
    }
#endif

    // If we found the default browser and we have support for that browser,
    // make sure to check it before any other browser, by moving it to the head
    // of the array.
    if (defaultBrowser)
      migratorsOrdered.sort(function(a, b) b == defaultBrowser ? 1 : 0);

    for (let i = 0; i < migratorsOrdered.length; i++) {
      let migrator = this._getMigratorIfSourceExists(migratorsOrdered[i]);
      if (migrator)
        return [migratorsOrdered[i], migrator];
    }

    return ["", null];
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProfileMigrator]),
  classDescription: "Profile Migrator",
  contractID: "@mozilla.org/toolkit/profile-migrator;1",
  classID: Components.ID("6F8BB968-C14F-4D6F-9733-6C6737B35DCE")
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory([ProfileMigrator]);
