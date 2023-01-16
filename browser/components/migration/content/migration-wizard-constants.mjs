/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const MigrationWizardConstants = Object.freeze({
  /**
   * A mapping of a page identification string to the IDs used by the
   * various wizard pages. These are used by MigrationWizard.setState
   * to set the current page.
   *
   * @type {Object<string, string>}
   */
  PAGES: Object.freeze({
    SELECTION: "selection",
    PROGRESS: "progress",
    SAFARI_PERMISSION: "safari-permission",
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

    // COOKIE resource migration is going to be removed, so we don't include
    // it here.
    HISTORY: "history",
    FORMDATA: "form-autofill",
    PASSWORDS: "logins-and-passwords",
    BOOKMARKS: "bookmarks",
    // We don't yet show OTHERDATA or SESSION resources.
  }),
});
