/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BookmarkJSONUtils: "resource://gre/modules/BookmarkJSONUtils.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "isBrowsingHistoryEnabled",
  "places.history.enabled",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "isSanitizeOnShutdownEnabled",
  "privacy.sanitize.sanitizeOnShutdown",
  false
);

/**
 * Class representing Places database related files within a user profile.
 */
export class PlacesBackupResource extends BackupResource {
  static get key() {
    return "places";
  }

  static get requiresEncryption() {
    return false;
  }

  async backup(stagingPath, profilePath = PathUtils.profileDir) {
    const sqliteDatabases = ["places.sqlite", "favicons.sqlite"];
    let canBackupHistory =
      !lazy.PrivateBrowsingUtils.permanentPrivateBrowsing &&
      !lazy.isSanitizeOnShutdownEnabled &&
      lazy.isBrowsingHistoryEnabled;

    /**
     * Do not backup places.sqlite and favicons.sqlite if users have history disabled, want history cleared on shutdown or are using permanent private browsing mode.
     * Instead, export all existing bookmarks to a compressed JSON file that we can read when restoring the backup.
     */
    if (!canBackupHistory) {
      let bookmarksBackupFile = PathUtils.join(
        stagingPath,
        "bookmarks.jsonlz4"
      );
      await lazy.BookmarkJSONUtils.exportToFile(bookmarksBackupFile, {
        compress: true,
      });
      return { bookmarksOnly: true };
    }

    await BackupResource.copySqliteDatabases(
      profilePath,
      stagingPath,
      sqliteDatabases
    );

    return null;
  }

  async measure(profilePath = PathUtils.profileDir) {
    let placesDBPath = PathUtils.join(profilePath, "places.sqlite");
    let faviconsDBPath = PathUtils.join(profilePath, "favicons.sqlite");
    let placesDBSize = await BackupResource.getFileSize(placesDBPath);
    let faviconsDBSize = await BackupResource.getFileSize(faviconsDBPath);

    Glean.browserBackup.placesSize.set(placesDBSize);
    Glean.browserBackup.faviconsSize.set(faviconsDBSize);
  }
}
