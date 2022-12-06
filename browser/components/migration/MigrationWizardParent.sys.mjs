/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MigrationUtils } from "resource:///modules/MigrationUtils.sys.mjs";
import { E10SUtils } from "resource://gre/modules/E10SUtils.sys.mjs";

/**
 * This class is responsible for communicating with MigrationUtils to do the
 * actual heavy-lifting of any kinds of migration work, based on messages from
 * the associated MigrationWizardChild.
 */
export class MigrationWizardParent extends JSWindowActorParent {
  /**
   * General message handler function for messages received from the
   * associated MigrationWizardChild JSWindowActor.
   *
   * @param {ReceiveMessageArgument} message
   *   The message received from the MigrationWizardChild.
   * @returns {Promise}
   */
  async receiveMessage(message) {
    // Some belt-and-suspenders here, mainly because the migration-wizard
    // component can be embedded in less privileged content pages, so let's
    // make sure that any messages from content are coming from the privileged
    // about content process type.
    if (
      !this.browsingContext.currentWindowGlobal.isInProcess &&
      this.browsingContext.currentRemoteType !=
        E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE
    ) {
      throw new Error(
        "MigrationWizardParent: received message from the wrong content process type."
      );
    }

    if (message.name == "GetAvailableMigrators") {
      let availableMigrators = new Map();
      for (const key of MigrationUtils.gAvailableMigratorKeys) {
        try {
          let migratorPromise = MigrationUtils.getMigrator(key).catch(
            console.error
          );
          if (migratorPromise) {
            availableMigrators.set(key, migratorPromise);
          }
        } catch (e) {
          console.error(`Could not get migrator with key ${key}`);
        }
      }
      // Wait for all getMigrator calls to resolve in parallel
      await Promise.all(availableMigrators.values());
      // ...and then filter out any that resolved to null.
      return Array.from(availableMigrators.keys()).filter(key => {
        return availableMigrators.get(key);
      });
    }
    return null;
  }
}
