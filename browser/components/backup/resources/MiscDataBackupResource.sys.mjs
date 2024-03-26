/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";

/**
 * Class representing miscellaneous files for telemetry, site storage,
 * media device origin mapping and Mozilla Accounts within a user profile.
 */
export class MiscDataBackupResource extends BackupResource {
  static get key() {
    return "miscellaneous";
  }

  async measure(profilePath = PathUtils.profileDir) {
    const files = [
      "times.json",
      "signedInUser.json",
      "enumerate_devices.txt",
      "protections.sqlite",
      "SiteSecurityServiceState.bin",
    ];

    let fullSize = 0;

    for (let filePath of files) {
      let resourcePath = PathUtils.join(profilePath, filePath);
      let resourceSize = await BackupResource.getFileSize(resourcePath);
      if (Number.isInteger(resourceSize)) {
        fullSize += resourceSize;
      }
    }

    Glean.browserBackup.miscDataSize.set(fullSize);
  }
}
