/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MigratorBase } from "resource:///modules/MigratorBase.sys.mjs";
import { MigrationWizardConstants } from "chrome://browser/content/migration/migration-wizard-constants.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  MigrationUtils: "resource:///modules/MigrationUtils.sys.mjs",
});

/**
 * A stub of a migrator used for automated testing only.
 */
export class InternalTestingProfileMigrator extends MigratorBase {
  static get key() {
    return "internal-testing";
  }

  static get displayNameL10nID() {
    return "Internal Testing Migrator";
  }

  static get sourceID() {
    return 1;
  }

  getSourceProfiles() {
    return Promise.resolve([InternalTestingProfileMigrator.testProfile]);
  }

  // We will create a single MigratorResource for each resource type that
  // just immediately reports a successful migration.
  getResources(aProfile) {
    if (
      !aProfile ||
      aProfile.id != InternalTestingProfileMigrator.testProfile.id
    ) {
      throw new Error(
        "InternalTestingProfileMigrator.getResources expects test profile."
      );
    }
    return Object.values(lazy.MigrationUtils.resourceTypes).map(type => {
      return {
        type,
        migrate: callback => {
          if (type == lazy.MigrationUtils.resourceTypes.EXTENSIONS) {
            callback(true, {
              progressValue: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
              totalExtensions: [],
              importedExtensions: [],
            });
          } else {
            callback(true /* success */);
          }
        },
      };
    });
  }

  /**
   * Clears the MigratorResources that are normally cached by the
   * MigratorBase parent class after a call to getResources. This
   * allows our automated tests to try different resource availability
   * scenarios between tests.
   */
  flushResourceCache() {
    this._resourcesByProfile = null;
  }

  static get testProfile() {
    return { id: "test-profile", name: "Some test profile" };
  }
}
