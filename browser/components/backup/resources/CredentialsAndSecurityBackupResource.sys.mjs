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

  static get requiresEncryption() {
    return true;
  }

  async backup(stagingPath, profilePath = PathUtils.profileDir) {
    const simpleCopyFiles = [
      "pkcs11.txt",
      "logins.json",
      "logins-backup.json",
      "autofill-profiles.json",
      "signedInUser.json",
    ];
    await BackupResource.copyFiles(profilePath, stagingPath, simpleCopyFiles);

    const sqliteDatabases = ["cert9.db", "key4.db", "credentialstate.sqlite"];
    await BackupResource.copySqliteDatabases(
      profilePath,
      stagingPath,
      sqliteDatabases
    );

    return null;
  }

  async recover(_manifestEntry, recoveryPath, destProfilePath) {
    const files = [
      "pkcs11.txt",
      "logins.json",
      "logins-backup.json",
      "autofill-profiles.json",
      "signedInUser.json",
      "cert9.db",
      "key4.db",
      "credentialstate.sqlite",
    ];
    await BackupResource.copyFiles(recoveryPath, destProfilePath, files);

    return null;
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
      "signedInUser.json",
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
