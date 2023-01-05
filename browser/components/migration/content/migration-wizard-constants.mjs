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
});
