/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MigrationUtils } from "resource:///modules/MigrationUtils.sys.mjs";

export function ProfileMigrator() {}

ProfileMigrator.prototype = {
  migrate: MigrationUtils.startupMigration.bind(MigrationUtils),
  QueryInterface: ChromeUtils.generateQI(["nsIProfileMigrator"]),
  classDescription: "Profile Migrator",
  contractID: "@mozilla.org/toolkit/profile-migrator;1",
  classID: Components.ID("6F8BB968-C14F-4D6F-9733-6C6737B35DCE"),
};
