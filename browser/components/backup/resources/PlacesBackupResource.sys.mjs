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

const BOOKMARKS_BACKUP_FILENAME = "bookmarks.jsonlz4";

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

  static get priority() {
    return 1;
  }

  async backup(stagingPath, profilePath = PathUtils.profileDir) {
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
        BOOKMARKS_BACKUP_FILENAME
      );
      await lazy.BookmarkJSONUtils.exportToFile(bookmarksBackupFile, {
        compress: true,
      });
      return { bookmarksOnly: true };
    }

    // These are copied in parallel because they're attached[1], and we don't
    // want them to get out of sync with one another.
    //
    // [1]: https://www.sqlite.org/lang_attach.html
    await Promise.all([
      BackupResource.copySqliteDatabases(profilePath, stagingPath, [
        "places.sqlite",
      ]),
      BackupResource.copySqliteDatabases(profilePath, stagingPath, [
        "favicons.sqlite",
      ]),
    ]);

    return null;
  }

  async recover(manifestEntry, recoveryPath, destProfilePath) {
    if (!manifestEntry) {
      const simpleCopyFiles = ["places.sqlite", "favicons.sqlite"];
      await BackupResource.copyFiles(
        recoveryPath,
        destProfilePath,
        simpleCopyFiles
      );
    } else {
      const { bookmarksOnly } = manifestEntry;

      /**
       * If the recovery file only has bookmarks backed up, pass the file path to postRecovery()
       * so that we can import all bookmarks into the new profile once it's been launched and restored.
       */
      if (bookmarksOnly) {
        let bookmarksBackupPath = PathUtils.join(
          recoveryPath,
          BOOKMARKS_BACKUP_FILENAME
        );
        return { bookmarksBackupPath };
      }
    }

    return null;
  }

  async postRecovery(postRecoveryEntry) {
    if (postRecoveryEntry?.bookmarksBackupPath) {
      await lazy.BookmarkJSONUtils.importFromFile(
        postRecoveryEntry.bookmarksBackupPath,
        {
          replace: true,
        }
      );
    }
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
