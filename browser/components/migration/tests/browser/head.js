/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MigrationWizardConstants } = ChromeUtils.importESModule(
  "chrome://browser/content/migration/migration-wizard-constants.mjs"
);

const DIALOG_URL =
  "chrome://browser/content/migration/migration-dialog-window.html";

/**
 * The withMigrationWizardDialog callback, called after the
 * dialog has loaded and the wizard is ready.
 *
 * @callback withMigrationWizardDialogCallback
 * @param {DOMWindow} window
 *   The content window of the migration wizard subdialog frame.
 * @returns {Promise<undefined>}
 */

/**
 * Opens the migration wizard HTML5 dialog in about:preferences in the
 * current window's selected tab, runs an async taskFn, and then
 * cleans up by loading about:blank in the tab before resolving.
 *
 * @param {withMigrationWizardDialogCallback} taskFn
 *   An async test function to be called while the migration wizard
 *   dialog is open.
 * @returns {Promise<undefined>}
 */
async function withMigrationWizardDialog(taskFn) {
  let migrationDialogPromise = waitForMigrationWizardDialogTab();
  await MigrationUtils.showMigrationWizard(window, {});
  let prefsBrowser = await migrationDialogPromise;

  try {
    await taskFn(prefsBrowser.contentWindow);
  } finally {
    if (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.getTabForBrowser(prefsBrowser));
    } else {
      BrowserTestUtils.loadURIString(prefsBrowser, "about:blank");
      await BrowserTestUtils.browserLoaded(prefsBrowser);
    }
  }
}

/**
 * Returns a Promise that resolves when an about:preferences tab opens
 * in the current window which loads the migration wizard dialog.
 * The Promise will wait until the migration wizard reports that it
 * is ready with the "MigrationWizard:Ready" event.
 *
 * @returns {Promise<browser>}
 *   Resolves with the about:preferences browser element.
 */
async function waitForMigrationWizardDialogTab() {
  let wizardReady = BrowserTestUtils.waitForEvent(
    window,
    "MigrationWizard:Ready"
  );

  let tab;
  if (gBrowser.selectedTab.isEmpty) {
    tab = gBrowser.selectedTab;
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, url => {
      return url.startsWith("about:preferences");
    });
  } else {
    tab = await BrowserTestUtils.waitForNewTab(gBrowser, url => {
      return url.startsWith("about:preferences");
    });
  }

  await wizardReady;
  info("Done waiting - migration subdialog loaded and ready.");

  return tab.linkedBrowser;
}
