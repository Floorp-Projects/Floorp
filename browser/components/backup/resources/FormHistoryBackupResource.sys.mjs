/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";

/**
 * Class representing Form history database within a user profile.
 */
export class FormHistoryBackupResource extends BackupResource {
  static get key() {
    return "formhistory";
  }

  async measure(profilePath = PathUtils.profileDir) {
    let formHistoryDBPath = PathUtils.join(profilePath, "formhistory.sqlite");
    let formHistorySize = await BackupResource.getFileSize(formHistoryDBPath);

    Glean.browserBackup.formHistorySize.set(formHistorySize);
  }
}
