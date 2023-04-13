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
        this.#sendTelemetryEvent("opened");

        this.#wizardEl = event.target;
        this.setComponentState({
          page: MigrationWizardConstants.PAGES.LOADING,
        });

        let migrators = await this.sendQuery("GetAvailableMigrators");
        if (!migrators.length) {
          this.setComponentState({
            page: MigrationWizardConstants.PAGES.NO_BROWSERS_FOUND,
          });
          this.#sendTelemetryEvent("no_browsers_found");
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
        let extraArgs = this.#recordBeginMigrationEvent(event.detail);

        let hasPermissions = await this.sendQuery("CheckPermissions", {
          key: event.detail.key,
        });
        if (!hasPermissions) {
          if (event.detail.key == "safari") {
            this.#sendTelemetryEvent("safari_perms");
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

        await this.beginMigration(event.detail, extraArgs);
        break;
      }

      case "MigrationWizard:RequestSafariPermissions": {
        let success = await this.sendQuery("RequestSafariPermissions");
        if (success) {
          let extraArgs = this.#constructExtraArgs(event.detail);
          await this.beginMigration(event.detail, extraArgs);
        }
        break;
      }
    }
  }

  /**
   * Sends a message to the parent actor to record Event Telemetry.
   *
   * @param {string} type
   *   The type of event being recorded.
   * @param {object} [args=null]
   *   Optional extra_args to supply for the event.
   */
  #sendTelemetryEvent(type, args) {
    this.sendAsyncMessage("RecordEvent", { type, args });
  }

  /**
   * Constructs extra arguments to pass to some Event Telemetry based
   * on the MigrationDetails passed up from the MigrationWizard.
   *
   * See migration-wizard.mjs for a definition of MigrationDetails.
   *
   * @param {object} migrationDetails
   *   A MigrationDetails object.
   * @returns {object}
   */
  #constructExtraArgs(migrationDetails) {
    let extraArgs = {
      migrator_key: migrationDetails.key,
      history: "0",
      formdata: "0",
      passwords: "0",
      bookmarks: "0",
      other: 0,
    };

    for (let type of migrationDetails.resourceTypes) {
      switch (type) {
        case MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY: {
          extraArgs.history = "1";
          break;
        }

        case MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA: {
          extraArgs.formdata = "1";
          break;
        }

        case MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS: {
          extraArgs.passwords = "1";
          break;
        }

        case MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS: {
          extraArgs.bookmarks = "1";
          break;
        }

        default: {
          extraArgs.other++;
        }
      }
    }

    // Event Telemetry extra arguments expect strings for every value, so
    // now we coerce our "other" count into a string.
    extraArgs.other = String(extraArgs.other);
    return extraArgs;
  }

  /**
   * This migration wizard combines a lot of steps (selecting the browser, profile,
   * resources, and starting the migration) into a single page. This helper method
   * records Event Telemetry for each of those actions at the same time when a
   * migration begins.
   *
   * This method returns the extra_args object that was constructed for the
   * resources_selected and migration_started event so that a
   * "migration_finished" event can use the same extra_args without
   * regenerating it.
   *
   * See migration-wizard.mjs for a definition of MigrationDetails.
   *
   * @param {object} migrationDetails
   *   A MigrationDetails object.
   * @returns {object}
   */
  #recordBeginMigrationEvent(migrationDetails) {
    this.#sendTelemetryEvent("browser_selected", {
      migrator_key: migrationDetails.key,
    });

    if (migrationDetails.profile) {
      this.#sendTelemetryEvent("profile_selected", {
        migrator_key: migrationDetails.key,
      });
    }

    let extraArgs = this.#constructExtraArgs(migrationDetails);

    extraArgs.configured = String(Number(migrationDetails.expandedDetails));
    this.#sendTelemetryEvent("resources_selected", extraArgs);
    delete extraArgs.configured;

    this.#sendTelemetryEvent("migration_started", extraArgs);
    return extraArgs;
  }

  /**
   * Sends a message to the parent actor to attempt a migration.
   *
   * See migration-wizard.mjs for a definition of MigrationDetails.
   *
   * @param {object} migrationDetails
   *   A MigrationDetails object.
   * @param {object} extraArgs
   *   Extra argument object to pass to the Event Telemetry for finishing
   *   the migration.
   * @returns {Promise<undefined>}
   *   Returns a Promise that resolves after the parent responds to the migration
   *   message.
   */
  async beginMigration(migrationDetails, extraArgs) {
    await this.sendQuery("Migrate", migrationDetails);
    this.#sendTelemetryEvent("migration_finished", extraArgs);

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
