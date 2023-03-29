/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MigrationWizardConstants } from "chrome://browser/content/migration/migration-wizard-constants.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "SHOW_IMPORT_ALL_PREF",
  "browser.migrate.content-modal.import-all.enabled",
  false
);

/**
 * This class is responsible for updating the state of a <migration-wizard>
 * component, and for listening for events from that component to perform
 * various migration functions.
 */
export class MigrationWizardChild extends JSWindowActorChild {
  #wizardEl = null;

  /**
   * General event handler function for events dispatched from the
   * <migration-wizard> component.
   *
   * @param {Event} event
   *   The DOM event being handled.
   * @returns {Promise}
   */
  async handleEvent(event) {
    switch (event.type) {
      case "MigrationWizard:RequestState": {
        this.#wizardEl = event.target;
        this.setComponentState({
          page: MigrationWizardConstants.PAGES.LOADING,
        });

        let migrators = await this.sendQuery("GetAvailableMigrators");
        if (!migrators.length) {
          this.setComponentState({
            page: MigrationWizardConstants.PAGES.NO_BROWSERS_FOUND,
          });
        } else {
          this.setComponentState({
            migrators,
            page: MigrationWizardConstants.PAGES.SELECTION,
            showImportAll: lazy.SHOW_IMPORT_ALL_PREF,
          });
        }

        this.#wizardEl.dispatchEvent(
          new this.contentWindow.CustomEvent("MigrationWizard:Ready", {
            bubbles: true,
          })
        );
        break;
      }

      case "MigrationWizard:BeginMigration": {
        let hasPermissions = await this.sendQuery("CheckPermissions", {
          key: event.detail.key,
        });
        if (!hasPermissions) {
          if (event.detail.key == "safari") {
            this.setComponentState({
              page: MigrationWizardConstants.PAGES.SAFARI_PERMISSION,
            });
          } else {
            console.error(
              `A migrator with key ${event.detail.key} needs permissions, ` +
                "and no UI exists for that right now."
            );
          }
          return;
        }

        await this.beginMigration(event.detail);
        break;
      }

      case "MigrationWizard:RequestSafariPermissions": {
        let success = await this.sendQuery("RequestSafariPermissions");
        if (success) {
          await this.beginMigration(event.detail);
        }
        break;
      }
    }
  }

  /**
   * Sends a message to the parent actor to attempt a migration.
   *
   * See migration-wizard.mjs for a definition of MigrationDetails.
   *
   * @param {object} migrationDetails
   *   A MigrationDetails object.
   * @returns {Promise<undefined>}
   *   Returns a Promise that resolves after the parent responds to the migration
   *   message.
   */
  async beginMigration(migrationDetails) {
    await this.sendQuery("Migrate", migrationDetails);
    this.#wizardEl.dispatchEvent(
      new this.contentWindow.CustomEvent("MigrationWizard:DoneMigration", {
        bubbles: true,
      })
    );
  }

  /**
   * General message handler function for messages received from the
   * associated MigrationWizardParent JSWindowActor.
   *
   * @param {ReceiveMessageArgument} message
   *   The message received from the MigrationWizardParent.
   */
  receiveMessage(message) {
    if (message.name == "UpdateProgress") {
      this.setComponentState({
        page: MigrationWizardConstants.PAGES.PROGRESS,
        progress: message.data.progress,
        key: message.data.key,
      });
    }
  }

  /**
   * Calls the `setState` method on the <migration-wizard> component. The
   * state is cloned into the execution scope of this.#wizardEl.
   *
   * @param {object} state The state object that a <migration-wizard>
   *   component expects. See the documentation for the element's setState
   *   method for more details.
   */
  setComponentState(state) {
    if (!this.#wizardEl) {
      return;
    }
    // We waive XrayWrappers in the event that the element is embedded in
    // a document without system privileges, like about:welcome.
    Cu.waiveXrays(this.#wizardEl).setState(
      Cu.cloneInto(
        state,
        // ownerGlobal doesn't exist in content windows.
        // eslint-disable-next-line mozilla/use-ownerGlobal
        this.#wizardEl.ownerDocument.defaultView
      )
    );
  }
}
