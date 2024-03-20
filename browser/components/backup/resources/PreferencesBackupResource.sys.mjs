/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";

/**
 * Class representing files that modify preferences and permissions within a user profile.
 */
export class PreferencesBackupResource extends BackupResource {
  static get key() {
    return "preferences";
  }

  async measure(profilePath = PathUtils.profileDir) {
    const files = [
      "prefs.js",
      "xulstore.json",
      "permissions.sqlite",
      "content-prefs.sqlite",
      "containers.json",
      "handlers.json",
      "search.json.mozlz4",
      "user.js",
      "chrome/userChrome.css",
      "chrome/userContent.css",
    ];
    let fullSize = 0;

    for (let filePath of files) {
      let resourcePath = PathUtils.joinRelative(profilePath, filePath);
      let resourceSize = await BackupResource.getFileSize(resourcePath);
      if (Number.isInteger(resourceSize)) {
        fullSize += resourceSize;
      }
    }

    Glean.browserBackup.preferencesSize.set(fullSize);
  }
}
