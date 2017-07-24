/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
const {Prefs} = Cu.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});
const MIGRATION_ENDED_EVENT = "Migration:Ended";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MigrationUtils", "resource:///modules/MigrationUtils.jsm");

this.ManualMigration = class ManualMigration {
  constructor() {
    Services.obs.addObserver(this, MIGRATION_ENDED_EVENT);
    this._prefs = new Prefs();
  }

  uninit() {
    Services.obs.removeObserver(this, MIGRATION_ENDED_EVENT);
  }

  isMigrationMessageExpired() {
    let migrationLastShownDate = new Date(this._prefs.get("migrationLastShownDate") * 1000);
    let today = new Date();
    // Round down to midnight.
    today = new Date(today.getFullYear(), today.getMonth(), today.getDate());

    if (migrationLastShownDate < today) {
      let migrationRemainingDays = this._prefs.get("migrationRemainingDays") - 1;

      this._prefs.set("migrationRemainingDays", migrationRemainingDays);
      // .valueOf returns a value that is too large to store so we need to divide by 1000.
      this._prefs.set("migrationLastShownDate", today.valueOf() / 1000);

      if (migrationRemainingDays <= 0) {
        return true;
      }
    }

    return false;
  }

  /**
   * While alreadyExpired is false the migration message is displayed and we also
   * keep checking if we should expire it. Broadcast expiration to store.
   *
   * @param {bool} alreadyExpired Pref flag that is false for the first 3 active days,
   *                              time in which we display the migration message to the user.
   */
  expireIfNecessary(alreadyExpired) {
    if (!alreadyExpired && this.isMigrationMessageExpired()) {
      this.expireMigration();
    }
  }

  expireMigration() {
    this.store.dispatch(ac.SetPref("migrationExpired", true));
  }

  /**
   * Event listener for migration wizard completion event.
   */
  observe() {
    this.expireMigration();
  }

  onAction(action) {
    switch (action.type) {
      case at.PREFS_INITIAL_VALUES:
        this.expireIfNecessary(action.data.migrationExpired);
        break;
      case at.MIGRATION_START:
        MigrationUtils.showMigrationWizard(action._target.browser.ownerGlobal, [MigrationUtils.MIGRATION_ENTRYPOINT_NEWTAB]);
        break;
      case at.MIGRATION_CANCEL:
        this.expireMigration();
        break;
      case at.UNINIT:
        this.uninit();
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["ManualMigration"];
