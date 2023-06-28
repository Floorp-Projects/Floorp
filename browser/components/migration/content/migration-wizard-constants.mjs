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
   * A mapping of a progress value string. These are used by
   * MigrationWizard.#onShowingProgress to update the UI accordingly.
   *
   * @type {Object<string, number>}
   */
  PROGRESS_VALUE: Object.freeze({
    LOADING: 1,
    SUCCESS: 2,
    WARNING: 3,
    INFO: 4,
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
    PAYMENT_METHODS: "PAYMENT_METHODS",
    EXTENSIONS: "EXTENSIONS",

    COOKIES: "COOKIES",
    SESSION: "SESSION",
    OTHERDATA: "OTHERDATA",
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
    BOOKMARKS_FROM_FILE: "BOOKMARKS_FROM_FILE",
  }),

  /**
   * Returns a mapping of a resource type to a string used to identify
   * the associated resource group in the wizard via a data-resource-type
   * attribute. The keys are for resource types that are only ever shown
   * for profile resets.
   *
   * @type {Object<string, string>}
   */
  PROFILE_RESET_ONLY_RESOURCE_TYPES: Object.freeze({
    COOKIES: "COOKIES",
    SESSION: "SESSION",
    OTHERDATA: "OTHERDATA",
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

  /**
   * The values that are set on the extension extra key for the
   * migration_finished telemetry event. The definition of that event
   * defines it as:
   *
   * "3" if all extensions were matched after import. "2" if only some
   * extensions were matched. "1" if none were matched, and "0" if extensions
   * weren't selected for import.
   *
   * @type {Object<string, string>}
   */
  EXTENSIONS_IMPORT_RESULT: Object.freeze({
    NOT_IMPORTED: "0",
    NONE_MATCHED: "1",
    PARTIAL_MATCH: "2",
    ALL_MATCHED: "3",
  }),
});
