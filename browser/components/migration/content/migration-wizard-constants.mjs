/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const MigrationWizardConstants = Object.freeze({
  MIGRATOR_TYPES: Object.freeze({
    BROWSER: "browser",
    FILE: "file",
  }),

  /**
   * A mapping of a page identification string to the IDs used by the
   * various wizard pages. These are used by MigrationWizard.setState
   * to set the current page.
   *
   * @type {Object<string, string>}
   */
  PAGES: Object.freeze({
    LOADING: "loading",
    SELECTION: "selection",
    PROGRESS: "progress",
    FILE_IMPORT_PROGRESS: "file-import-progress",
    SAFARI_PERMISSION: "safari-permission",
    SAFARI_PASSWORD_PERMISSION: "safari-password-permission",
    NO_BROWSERS_FOUND: "no-browsers-found",
  }),

  /**
   * Returns a mapping of a resource type to a string used to identify
   * the associated resource group in the wizard via a data-resource-type
   * attribute. The keys are used to set which items should be shown and
   * in what state in #onShowingProgress.
   *
   * @type {Object<string, string>}
   */
  DISPLAYED_RESOURCE_TYPES: Object.freeze({
    // The DISPLAYED_RESOURCE_TYPES should have their keys match those
    // in MigrationUtils.resourceTypes.

    // This is a little silly, but JavaScript doesn't have a notion of
    // enums. The advantage of this set-up is that these constants values
    // can be used to access the MigrationUtils.resourceTypes constants,
    // are reasonably readable as DOM attributes, and easily serialize /
    // deserialize.
    HISTORY: "HISTORY",
    FORMDATA: "FORMDATA",
    PASSWORDS: "PASSWORDS",
    BOOKMARKS: "BOOKMARKS",

    // We don't yet show OTHERDATA or SESSION resources.
  }),

  DISPLAYED_FILE_RESOURCE_TYPES: Object.freeze({
    // When migrating passwords from a file, we first show the progress
    // for a single PASSWORDS_FROM_FILE resource type, and then upon
    // completion, show two different resource types - one for new
    // passwords imported from the file, and one for existing passwords
    // that were updated from the file.
    PASSWORDS_FROM_FILE: "PASSWORDS_FROM_FILE",
    PASSWORDS_NEW: "PASSWORDS_NEW",
    PASSWORDS_UPDATED: "PASSWORDS_UPDATED",
  }),

  /**
   * The set of keys that maps to migrators that use the term "favorites"
   * in the place of "bookmarks". This tends to be browsers from Microsoft.
   */
  USES_FAVORITES: Object.freeze([
    "chromium-edge",
    "chromium-edge-beta",
    "edge",
    "ie",
  ]),
});
