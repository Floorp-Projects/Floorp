/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MigrationWizardConstants } = ChromeUtils.importESModule(
  "chrome://browser/content/migration/migration-wizard-constants.mjs"
);

/**
 * Returns the constant strings from MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES
 * that aren't also part of MigrationWizardConstants.PROFILE_RESET_ONLY_RESOURCE_TYPES.
 *
 * This is the set of resources that the user can actually choose to migrate via
 * checkboxes.
 *
 * @returns {string[]}
 */
function getChoosableResourceTypes() {
  return Object.keys(MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES).filter(
    resourceType =>
      !MigrationWizardConstants.PROFILE_RESET_ONLY_RESOURCE_TYPES[resourceType]
  );
}
