/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";

/**
 * Class representing Places database related files within a user profile.
 */
export class PlacesBackupResource extends BackupResource {
  static get key() {
    return "places";
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
