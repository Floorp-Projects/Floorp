/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {actionCreators: ac, actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});

const MIGRATION_ENDED_EVENT = "Migration:Ended";
const MS_PER_DAY = 86400000;

ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "MigrationUtils", "resource:///modules/MigrationUtils.jsm");
ChromeUtils.defineModuleGetter(this, "ProfileAge", "resource://gre/modules/ProfileAge.jsm");

this.ManualMigration = class ManualMigration {
  constructor() {
    Services.obs.addObserver(this, MIGRATION_ENDED_EVENT);
  }

  get migrationLastShownDate() {
    return this.store.getState().Prefs.values.migrationLastShownDate;
  }

  set migrationLastShownDate(newDate) {
    this.store.dispatch(ac.SetPref("migrationLastShownDate", newDate));
  }

  get migrationRemainingDays() {
    return this.store.getState().Prefs.values.migrationRemainingDays;
  }

  set migrationRemainingDays(newDate) {
    this.store.dispatch(ac.SetPref("migrationRemainingDays", newDate));
  }

  uninit() {
    Services.obs.removeObserver(this, MIGRATION_ENDED_EVENT);
  }

  async isMigrationMessageExpired() {
    let profileAge = await ProfileAge();
    let profileCreationDate = await profileAge.created;
    let daysSinceProfileCreation = (Date.now() - profileCreationDate) / MS_PER_DAY;

    // We don't want to show the migration message to profiles older than 3 days.
    if (daysSinceProfileCreation > 3) {
      return true;
    }

    let migrationLastShownDate = new Date(this.migrationLastShownDate * 1000);
    let today = new Date();
    // Round down to midnight.
    today = new Date(today.getFullYear(), today.getMonth(), today.getDate());
    if (migrationLastShownDate < today) {
      let migrationRemainingDays = this.migrationRemainingDays - 1;

      this.migrationRemainingDays = migrationRemainingDays;

      // .valueOf returns a value that is too large to store so we need to divide by 1000.
      this.migrationLastShownDate = today.valueOf() / 1000;

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
  async expireIfNecessary(alreadyExpired) {
    if (!alreadyExpired && await this.isMigrationMessageExpired()) {
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
    this.store.dispatch({type: at.MIGRATION_COMPLETED});
  }

  async onAction(action) {
    switch (action.type) {
      case at.PREFS_INITIAL_VALUES:
        await this.expireIfNecessary(action.data.migrationExpired);
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

const EXPORTED_SYMBOLS = ["ManualMigration"];
