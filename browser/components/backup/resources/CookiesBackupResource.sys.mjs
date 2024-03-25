/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";

/**
 * Class representing Cookies database within a user profile.
 */
export class CookiesBackupResource extends BackupResource {
  static get key() {
    return "cookies";
  }

  async measure(profilePath = PathUtils.profileDir) {
    let cookiesDBPath = PathUtils.join(profilePath, "cookies.sqlite");
    let cookiesSize = await BackupResource.getFileSize(cookiesDBPath);

    Glean.browserBackup.cookiesSize.set(cookiesSize);
  }
}
