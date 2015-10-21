/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource:///modules/MSMigrationUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

const kEdgeRegistryRoot = "SOFTWARE\\Classes\\Local Settings\\Software\\" +
  "Microsoft\\Windows\\CurrentVersion\\AppContainer\\Storage\\" +
  "microsoft.microsoftedge_8wekyb3d8bbwe\\MicrosoftEdge";

function EdgeTypedURLMigrator() {
}

EdgeTypedURLMigrator.prototype = {
  type: MigrationUtils.resourceTypes.HISTORY,

  get _typedURLs() {
    if (!this.__typedURLs) {
      this.__typedURLs = MSMigrationUtils.getTypedURLs(kEdgeRegistryRoot);
    }
    return this.__typedURLs;
  },

  get exists() {
    return this._typedURLs.size > 0;
  },

  migrate: function(aCallback) {
    let rv = true;
    let typedURLs = this._typedURLs;
    let places = [];
    for (let [urlString, time] of typedURLs) {
      let uri;
      try {
        uri = Services.io.newURI(urlString, null, null);
        if (["http", "https", "ftp"].indexOf(uri.scheme) == -1) {
          continue;
        }
      } catch (ex) {
        Cu.reportError(ex);
        continue;
      }

      // Note that the time will be in microseconds (PRTime),
      // and Date.now() returns milliseconds. Places expects PRTime,
      // so we multiply the Date.now return value to make up the difference.
      let visitDate = time || (Date.now() * 1000);
      places.push({
        uri,
        visits: [{ transitionType: Ci.nsINavHistoryService.TRANSITION_TYPED,
                   visitDate}]
      });
    }

    if (places.length == 0) {
      aCallback(typedURLs.size == 0);
      return;
    }

    PlacesUtils.asyncHistory.updatePlaces(places, {
      _success: false,
      handleResult: function() {
        // Importing any entry is considered a successful import.
        this._success = true;
      },
      handleError: function() {},
      handleCompletion: function() {
        aCallback(this._success);
      }
    });
  },
}

function EdgeProfileMigrator() {
}

EdgeProfileMigrator.prototype = Object.create(MigratorPrototype);

EdgeProfileMigrator.prototype.getResources = function() {
  let resources = [
    MSMigrationUtils.getBookmarksMigrator(MSMigrationUtils.MIGRATION_TYPE_EDGE),
    MSMigrationUtils.getCookiesMigrator(MSMigrationUtils.MIGRATION_TYPE_EDGE),
    new EdgeTypedURLMigrator(),
  ];
  let windowsVaultFormPasswordsMigrator =
    MSMigrationUtils.getWindowsVaultFormPasswordsMigrator();
  windowsVaultFormPasswordsMigrator.name = "EdgeVaultFormPasswords";
  resources.push(windowsVaultFormPasswordsMigrator);
  return resources.filter(r => r.exists);
};

/* Somewhat counterintuitively, this returns:
 * - |null| to indicate "There is only 1 (default) profile" (on win10+)
 * - |[]| to indicate "There are no profiles" (on <=win8.1) which will avoid using this migrator.
 * See MigrationUtils.jsm for slightly more info on how sourceProfiles is used.
 */
EdgeProfileMigrator.prototype.__defineGetter__("sourceProfiles", function() {
  let isWin10OrHigher = AppConstants.isPlatformAndVersionAtLeast("win", "10");
  return isWin10OrHigher ? null : [];
});

EdgeProfileMigrator.prototype.classDescription = "Edge Profile Migrator";
EdgeProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=edge";
EdgeProfileMigrator.prototype.classID = Components.ID("{62e8834b-2d17-49f5-96ff-56344903a2ae}");

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([EdgeProfileMigrator]);
