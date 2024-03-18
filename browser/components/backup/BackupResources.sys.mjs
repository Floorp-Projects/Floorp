/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/**
 * Classes exported here are registered as a resource that can be
 * backed up and restored in the BackupService.
 *
 * They must extend the BackupResource base class.
 */
import { PlacesBackupResource } from "resource:///modules/backup/PlacesBackupResource.sys.mjs";

export { PlacesBackupResource };
