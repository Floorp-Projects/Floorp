/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";

/**
 * Class representing files needed for logins, payment methods and form autofill within a user profile.
 */
export class CredentialsAndSecurityBackupResource extends BackupResource {
  static get key() {
    return "credentials_and_security";
  }

  async measure(profilePath = PathUtils.profileDir) {
    const securityFiles = ["cert9.db", "pkcs11.txt"];
    let securitySize = 0;

    for (let filePath of securityFiles) {
      let resourcePath = PathUtils.join(profilePath, filePath);
      let resourceSize = await BackupResource.getFileSize(resourcePath);
      if (Number.isInteger(resourceSize)) {
        securitySize += resourceSize;
      }
    }

    Glean.browserBackup.securityDataSize.set(securitySize);

    const credentialsFiles = [
      "key4.db",
      "logins.json",
      "logins-backup.json",
      "autofill-profiles.json",
      "credentialstate.sqlite",
    ];
    let credentialsSize = 0;

    for (let filePath of credentialsFiles) {
      let resourcePath = PathUtils.join(profilePath, filePath);
      let resourceSize = await BackupResource.getFileSize(resourcePath);
      if (Number.isInteger(resourceSize)) {
        credentialsSize += resourceSize;
      }
    }

    Glean.browserBackup.credentialsDataSize.set(credentialsSize);
  }
}
