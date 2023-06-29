/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { MigrationUtils } = ChromeUtils.importESModule(
  "resource:///modules/MigrationUtils.sys.mjs"
);
const { MigratorBase } = ChromeUtils.importESModule(
  "resource:///modules/MigratorBase.sys.mjs"
);

/**
 * Map from data types that match Ci.nsIBrowserProfileMigrator's types to
 * prefixes for strings used to label these data types in the migration
 * dialog. We use these strings with -checkbox and -label suffixes for the
 * checkboxes on the "importItems" page, and for the labels on the "migrating"
 * and "done" pages, respectively.
 */
const kDataToStringMap = new Map([
  ["cookies", "browser-data-cookies"],
  ["history", "browser-data-history"],
  ["formdata", "browser-data-formdata"],
  ["passwords", "browser-data-passwords"],
  ["bookmarks", "browser-data-bookmarks"],
  ["otherdata", "browser-data-otherdata"],
  ["session", "browser-data-session"],
  ["payment_methods", "browser-data-payment-methods"],
]);

var MigrationWizard = {
  /* exported MigrationWizard */
  _source: "", // Source Profile Migrator ContractID suffix
  _itemsFlags: MigrationUtils.resourceTypes.ALL, // Selected Import Data Sources (16-bit bitfield)
  _selectedProfile: null, // Selected Profile name to import from
  _wiz: null,
  _migrator: null,
  _autoMigrate: null,
  _receivedPermissions: new Set(),
  _succeededMigrationEventArgs: null,
  _openedTime: null,

  init() {
    Services.telemetry.setEventRecordingEnabled("browser.migration", true);

    let os = Services.obs;
    os.addObserver(this, "Migration:Started");
    os.addObserver(this, "Migration:ItemBeforeMigrate");
    os.addObserver(this, "Migration:ItemAfterMigrate");
    os.addObserver(this, "Migration:ItemError");
    os.addObserver(this, "Migration:Ended");

    this._wiz = document.querySelector("wizard");

    let args = window.arguments[0]?.wrappedJSObject || {};
    let entrypoint =
      args.entrypoint || MigrationUtils.MIGRATION_ENTRYPOINTS.UNKNOWN;
    Services.telemetry
      .getHistogramById("FX_MIGRATION_ENTRY_POINT_CATEGORICAL")
      .add(entrypoint);

    // The legacy entrypoint Histogram wasn't categorical, so we translate to the right
    // numeric value before writing it. We'll keep this Histogram around to ensure a
    // smooth transition to the new FX_MIGRATION_ENTRY_POINT_CATEGORICAL categorical
    // histogram.
    let entryPointId = MigrationUtils.getLegacyMigrationEntrypoint(entrypoint);
    Services.telemetry
      .getHistogramById("FX_MIGRATION_ENTRY_POINT")
      .add(entryPointId);

    // If the caller passed openedTime, that means this is the first time that
    // the migration wizard is opening, and we want to measure its performance.
    // Stash the time that opening was invoked so that we can measure the
    // total elapsed time when the source list is shown.
    if (args.openedTime) {
      this._openedTime = args.openedTime;
    }

    this.isInitialMigration =
      entrypoint == MigrationUtils.MIGRATION_ENTRYPOINTS.FIRSTRUN;

    // Record that the uninstaller requested a profile refresh
    if (Services.env.get("MOZ_UNINSTALLER_PROFILE_REFRESH")) {
      Services.env.set("MOZ_UNINSTALLER_PROFILE_REFRESH", "");
      Services.telemetry.scalarSet(
        "migration.uninstaller_profile_refresh",
        true
      );
    }

    this._source = args.migratorKey;
    this._migrator =
      args.migrator instanceof MigratorBase ? args.migrator : null;
    this._autoMigrate = !!args.isStartupMigration;
    this._skipImportSourcePage = !!args.skipSourceSelection;

    if (this._migrator && args.profileId) {
      let sourceProfiles = this.spinResolve(this._migrator.getSourceProfiles());
      this._selectedProfile = sourceProfiles.find(
        profile => profile.id == args.profileId
      );
    }

    if (this._autoMigrate) {
      // Show the "nothing" option in the automigrate case to provide an
      // easily identifiable way to avoid migration and create a new profile.
      document.getElementById("nothing").hidden = false;
    }

    this._setSourceForDataLocalization();

    document.addEventListener("wizardcancel", function () {
      MigrationWizard.onWizardCancel();
    });

    document
      .getElementById("selectProfile")
      .addEventListener("pageshow", function () {
        MigrationWizard.onSelectProfilePageShow();
      });
    document
      .getElementById("importItems")
      .addEventListener("pageshow", function () {
        MigrationWizard.onImportItemsPageShow();
      });
    document
      .getElementById("migrating")
      .addEventListener("pageshow", function () {
        MigrationWizard.onMigratingPageShow();
      });
    document.getElementById("done").addEventListener("pageshow", function () {
      MigrationWizard.onDonePageShow();
    });

    document
      .getElementById("selectProfile")
      .addEventListener("pagerewound", function () {
        MigrationWizard.onSelectProfilePageRewound();
      });
    document
      .getElementById("importItems")
      .addEventListener("pagerewound", function () {
        MigrationWizard.onImportItemsPageRewound();
      });

    document
      .getElementById("selectProfile")
      .addEventListener("pageadvanced", function () {
        MigrationWizard.onSelectProfilePageAdvanced();
      });
    document
      .getElementById("importItems")
      .addEventListener("pageadvanced", function () {
        MigrationWizard.onImportItemsPageAdvanced();
      });
    document
      .getElementById("importPermissions")
      .addEventListener("pageadvanced", function (e) {
        MigrationWizard.onImportPermissionsPageAdvanced(e);
      });
    document
      .getElementById("importSource")
      .addEventListener("pageadvanced", function (e) {
        MigrationWizard.onImportSourcePageAdvanced(e);
      });

    this.recordEvent("opened");

    this.onImportSourcePageShow();
  },

  uninit() {
    var os = Services.obs;
    os.removeObserver(this, "Migration:Started");
    os.removeObserver(this, "Migration:ItemBeforeMigrate");
    os.removeObserver(this, "Migration:ItemAfterMigrate");
    os.removeObserver(this, "Migration:ItemError");
    os.removeObserver(this, "Migration:Ended");
    os.notifyObservers(this, "MigrationWizard:Destroyed");
    MigrationUtils.finishMigration();
  },

  /**
   * Used for recording telemetry in the migration wizard.
   *
   * @param {string} type
   *   The type of event being recorded.
   * @param {object} args
   *   The data to pass to telemetry when the event is recorded.
   */
  recordEvent(type, args = null) {
    Services.telemetry.recordEvent(
      "browser.migration",
      type,
      "legacy_wizard",
      null,
      args
    );
  },

  spinResolve(promise) {
    let canAdvance = this._wiz.canAdvance;
    let canRewind = this._wiz.canRewind;
    this._wiz.canAdvance = false;
    this._wiz.canRewind = false;
    let result = MigrationUtils.spinResolve(promise);
    this._wiz.canAdvance = canAdvance;
    this._wiz.canRewind = canRewind;
    return result;
  },

  _setSourceForDataLocalization() {
    this._sourceForDataLocalization = this._source;
    // Ensure consistency for various channels, brandings and versions of
    // Chromium and MS Edge.
    if (this._sourceForDataLocalization) {
      this._sourceForDataLocalization = this._sourceForDataLocalization
        .replace(/^(chromium-edge-beta|chromium-edge)$/, "edge")
        .replace(/^(canary|chromium|chrome-beta|chrome-dev)$/, "chrome");
    }
  },

  onWizardCancel() {
    MigrationUtils.forceExitSpinResolve();
    return true;
  },

  // 1 - Import Source
  onImportSourcePageShow() {
    this._wiz.canRewind = false;

    var selectedMigrator = null;
    this._availableMigrators = [];

    // Figure out what source apps are are available to import from:
    var group = document.getElementById("importSourceGroup");
    for (var i = 0; i < group.childNodes.length; ++i) {
      var migratorKey = group.childNodes[i].id;
      if (migratorKey != "nothing") {
        var migrator = this.spinResolve(
          MigrationUtils.getMigrator(migratorKey)
        );

        if (migrator?.enabled) {
          // Save this as the first selectable item, if we don't already have
          // one, or if it is the migrator that was passed to us.
          if (!selectedMigrator || this._source == migratorKey) {
            selectedMigrator = group.childNodes[i];
          }

          let profiles = this.spinResolve(migrator.getSourceProfiles());
          if (profiles?.length) {
            Services.telemetry.keyedScalarAdd(
              "migration.discovered_migrators",
              migratorKey,
              profiles.length
            );
          } else {
            Services.telemetry.keyedScalarAdd(
              "migration.discovered_migrators",
              migratorKey,
              1
            );
          }

          this._availableMigrators.push([migratorKey, migrator]);
        } else {
          // Hide this option
          group.childNodes[i].hidden = true;
        }
      }
    }
    if (this.isInitialMigration) {
      Services.telemetry
        .getHistogramById("FX_STARTUP_MIGRATION_BROWSER_COUNT")
        .add(this._availableMigrators.length);
      let defaultBrowser = MigrationUtils.getMigratorKeyForDefaultBrowser();
      // This will record 0 for unknown default browser IDs.
      defaultBrowser = MigrationUtils.getSourceIdForTelemetry(defaultBrowser);
      Services.telemetry
        .getHistogramById("FX_STARTUP_MIGRATION_EXISTING_DEFAULT_BROWSER")
        .add(defaultBrowser);
    }

    if (selectedMigrator) {
      group.selectedItem = selectedMigrator;
    } else {
      this.recordEvent("no_browsers_found");
      // We didn't find a migrator, notify the user
      document.getElementById("noSources").hidden = false;

      this._wiz.canAdvance = false;

      document.getElementById("importAll").hidden = true;
    }

    // This must be the first time we're opening the migration wizard,
    // and we want to know how long it took to get to this point, where
    // we're showing the source list.
    if (this._openedTime !== null) {
      let elapsed = Cu.now() - this._openedTime;
      Services.telemetry.scalarSet(
        "migration.time_to_produce_legacy_migrator_list",
        elapsed
      );
    }

    // Advance to the next page if the caller told us to.
    if (this._migrator && this._skipImportSourcePage) {
      this._wiz.advance();
      this._wiz.canRewind = false;
    }
  },

  onImportSourcePageAdvanced(event) {
    var newSource =
      document.getElementById("importSourceGroup").selectedItem.id;

    this.recordEvent("browser_selected", { migrator_key: newSource });

    if (newSource == "nothing") {
      // Need to do telemetry here because we're closing the dialog before we get to
      // do actual migration. For actual migration, this doesn't happen until after
      // migration takes place.
      Services.telemetry
        .getHistogramById("FX_MIGRATION_SOURCE_BROWSER")
        .add(MigrationUtils.getSourceIdForTelemetry("nothing"));
      this._wiz.cancel();
      event.preventDefault();
    }

    if (!this._migrator || newSource != this._source) {
      // Create the migrator for the selected source.
      this._migrator = this.spinResolve(MigrationUtils.getMigrator(newSource));

      this._itemsFlags = MigrationUtils.resourceTypes.ALL;
      this._selectedProfile = null;
    }
    this._source = newSource;
    this._setSourceForDataLocalization();

    // check for more than one source profile
    var sourceProfiles = this.spinResolve(this._migrator.getSourceProfiles());
    if (this._skipImportSourcePage) {
      this._updateNextPageForPermissions();
    } else if (sourceProfiles && sourceProfiles.length > 1) {
      this._wiz.currentPage.next = "selectProfile";
    } else {
      if (this._autoMigrate) {
        this._updateNextPageForPermissions();
      } else {
        this._wiz.currentPage.next = "importItems";
      }

      if (sourceProfiles && sourceProfiles.length == 1) {
        this._selectedProfile = sourceProfiles[0];
      } else {
        this._selectedProfile = null;
      }
    }
  },

  // 2 - [Profile Selection]
  onSelectProfilePageShow() {
    // Disabling this for now, since we ask about import sources in automigration
    // too and don't want to disable the back button
    // if (this._autoMigrate)
    //   document.documentElement.getButton("back").disabled = true;

    var profiles = document.getElementById("profiles");
    while (profiles.hasChildNodes()) {
      profiles.firstChild.remove();
    }

    // Note that this block is still reached even if the user chose 'From File'
    // and we canceled the dialog.  When that happens, _migrator will be null.
    if (this._migrator) {
      var sourceProfiles = this.spinResolve(this._migrator.getSourceProfiles());

      for (let profile of sourceProfiles) {
        var item = document.createXULElement("radio");
        item.id = profile.id;
        item.setAttribute("label", profile.name);
        profiles.appendChild(item);
      }
    }

    profiles.selectedItem = this._selectedProfile
      ? document.getElementById(this._selectedProfile.id)
      : profiles.firstChild;
  },

  onSelectProfilePageRewound() {
    var profiles = document.getElementById("profiles");
    let sourceProfiles = this.spinResolve(this._migrator.getSourceProfiles());
    this._selectedProfile =
      sourceProfiles.find(profile => profile.id == profiles.selectedItem.id) ||
      null;
  },

  onSelectProfilePageAdvanced() {
    this.recordEvent("profile_selected", {
      migrator_key: this._source,
    });
    var profiles = document.getElementById("profiles");
    let sourceProfiles = this.spinResolve(this._migrator.getSourceProfiles());
    this._selectedProfile =
      sourceProfiles.find(profile => profile.id == profiles.selectedItem.id) ||
      null;

    // If we're automigrating or just doing bookmarks don't show the item selection page
    if (this._autoMigrate) {
      this._updateNextPageForPermissions();
    }
  },

  // 3 - ImportItems
  onImportItemsPageShow() {
    var dataSources = document.getElementById("dataSources");
    while (dataSources.hasChildNodes()) {
      dataSources.firstChild.remove();
    }

    var items = this.spinResolve(
      this._migrator.getMigrateData(this._selectedProfile)
    );

    for (let itemType of kDataToStringMap.keys()) {
      let itemValue = MigrationUtils.resourceTypes[itemType.toUpperCase()];
      if (items & itemValue) {
        let checkbox = document.createXULElement("checkbox");
        checkbox.id = itemValue;
        checkbox.setAttribute("native", true);
        document.l10n.setAttributes(
          checkbox,
          kDataToStringMap.get(itemType) + "-checkbox",
          { browser: this._sourceForDataLocalization }
        );
        dataSources.appendChild(checkbox);
        if (!this._itemsFlags || this._itemsFlags & itemValue) {
          checkbox.checked = true;
        }
      }
    }
  },

  onImportItemsPageRewound() {
    this._wiz.canAdvance = true;
    this.onImportItemsPageAdvanced(true /* viaRewind */);
  },

  onImportItemsPageAdvanced(viaRewind = false) {
    let extraKeys = {
      migrator_key: this._source,
      history: "0",
      formdata: "0",
      passwords: "0",
      bookmarks: "0",
      payment_methods: "0",

      // "other" will get incremented, so we keep this as a number for
      // now, and will cast to a string before submitting to Event telemetry.
      other: 0,

      configured: "0",
    };

    var dataSources = document.getElementById("dataSources");
    this._itemsFlags = 0;

    for (var i = 0; i < dataSources.childNodes.length; ++i) {
      var checkbox = dataSources.childNodes[i];
      if (checkbox.localName == "checkbox" && checkbox.checked) {
        let flag = parseInt(checkbox.id);

        switch (flag) {
          case MigrationUtils.resourceTypes.HISTORY:
            extraKeys.history = "1";
            break;
          case MigrationUtils.resourceTypes.FORMDATA:
            extraKeys.formdata = "1";
            break;
          case MigrationUtils.resourceTypes.PASSWORDS:
            extraKeys.passwords = "1";
            break;
          case MigrationUtils.resourceTypes.BOOKMARKS:
            extraKeys.bookmarks = "1";
            break;
          case MigrationUtils.resourceTypes.PAYMENT_METHODS:
            extraKeys.payment_methods = "1";
            break;
          default:
            extraKeys.other++;
        }

        this._itemsFlags |= parseInt(checkbox.id);
      }
    }

    extraKeys.other = String(extraKeys.other);

    if (!viaRewind) {
      this.recordEvent("resources_selected", extraKeys);
    }

    this._updateNextPageForPermissions();
  },

  onImportItemCommand() {
    var items = document.getElementById("dataSources");
    var checkboxes = items.getElementsByTagName("checkbox");

    var oneChecked = false;
    for (var i = 0; i < checkboxes.length; ++i) {
      if (checkboxes[i].checked) {
        oneChecked = true;
        break;
      }
    }

    this._wiz.canAdvance = oneChecked;

    this._updateNextPageForPermissions();
  },

  _updateNextPageForPermissions() {
    // We would like to just go straight to work:
    this._wiz.currentPage.next = "migrating";
    // If we already have permissions, this is easy:
    if (this._receivedPermissions.has(this._source)) {
      return;
    }

    // Otherwise, if we're on mojave or later and importing from
    // Safari, prompt for the bookmarks file.
    // We may add other browser/OS combos here in future.
    if (
      this._source == "safari" &&
      AppConstants.isPlatformAndVersionAtLeast("macosx", "18") &&
      (this._itemsFlags & MigrationUtils.resourceTypes.BOOKMARKS ||
        this._itemsFlags == MigrationUtils.resourceTypes.ALL)
    ) {
      let havePermissions = this.spinResolve(this._migrator.hasPermissions());

      if (!havePermissions) {
        this._wiz.currentPage.next = "importPermissions";
        this.recordEvent("safari_perms");
      }
    }
  },

  // 3b: permissions. This gets invoked when the user clicks "Next"
  async onImportPermissionsPageAdvanced(event) {
    // We're done if we have permission:
    if (this._receivedPermissions.has(this._source)) {
      return;
    }
    // The wizard helper is sync, and we need to check some stuff, so just stop
    // advancing for now and prompt the user, then advance the wizard if everything
    // worked.
    event.preventDefault();

    await this._migrator.getPermissions(window);
    if (await this._migrator.hasPermissions()) {
      this._receivedPermissions.add(this._source);
      // Re-enter (we'll then allow the advancement through the early return above)
      this._wiz.advance();
    }
    // if we didn't have permissions after the `getPermissions` call, the user
    // cancelled the dialog. Just no-op out now; the user can re-try by clicking
    // the 'Continue' button again, or go back and pick a different browser.
  },

  // 4 - Migrating
  onMigratingPageShow() {
    this._wiz.getButton("cancel").disabled = true;
    this._wiz.canRewind = false;
    this._wiz.canAdvance = false;

    // When automigrating, show all of the data that can be received from this source.
    if (this._autoMigrate) {
      this._itemsFlags = this.spinResolve(
        this._migrator.getMigrateData(this._selectedProfile)
      );
    }

    this._listItems("migratingItems");
    setTimeout(() => this.onMigratingMigrate(), 0);
  },

  async onMigratingMigrate() {
    await this._migrator.migrate(
      this._itemsFlags,
      this._autoMigrate,
      this._selectedProfile
    );

    Services.telemetry
      .getHistogramById("FX_MIGRATION_SOURCE_BROWSER")
      .add(MigrationUtils.getSourceIdForTelemetry(this._source));
    if (!this._autoMigrate) {
      let hist = Services.telemetry.getKeyedHistogramById("FX_MIGRATION_USAGE");
      let exp = 0;
      let items = this._itemsFlags;
      while (items) {
        if (items & 1) {
          hist.add(this._source, exp);
        }
        items = items >> 1;
        exp++;
      }
    }
  },

  _listItems(aID) {
    var items = document.getElementById(aID);
    while (items.hasChildNodes()) {
      items.firstChild.remove();
    }

    for (let itemType of kDataToStringMap.keys()) {
      let itemValue = MigrationUtils.resourceTypes[itemType.toUpperCase()];
      if (this._itemsFlags & itemValue) {
        var label = document.createXULElement("label");
        label.id = itemValue + "_migrated";
        try {
          document.l10n.setAttributes(
            label,
            kDataToStringMap.get(itemType) + "-label",
            { browser: this._sourceForDataLocalization }
          );
          items.appendChild(label);
        } catch (e) {
          // if the block above throws, we've enumerated all the import data types we
          // currently support and are now just wasting time, break.
          break;
        }
      }
    }
  },

  recordResourceMigration(obj, resourceType) {
    // Sometimes, the resourceType that gets passed here is a string, which
    // is bizarre. We'll hold our nose and accept either a string or a
    // number.
    resourceType = parseInt(resourceType, 10);

    switch (resourceType) {
      case MigrationUtils.resourceTypes.HISTORY:
        obj.history = "1";
        break;
      case MigrationUtils.resourceTypes.FORMDATA:
        obj.formdata = "1";
        break;
      case MigrationUtils.resourceTypes.PASSWORDS:
        obj.passwords = "1";
        break;
      case MigrationUtils.resourceTypes.BOOKMARKS:
        obj.bookmarks = "1";
        break;
      case MigrationUtils.resourceTypes.PAYMENT_METHODS:
        obj.payment_methods = "1";
        break;
      default:
        obj.other++;
    }
  },

  recordMigrationStartEvent(resourceFlags) {
    let extraKeys = {
      migrator_key: this._source,
      history: "0",
      formdata: "0",
      passwords: "0",
      bookmarks: "0",
      payment_methods: "0",
      // "other" will get incremented, so we keep this as a number for
      // now, and will cast to a string before submitting to Event telemetry.
      other: 0,
    };

    for (let resourceTypeKey in MigrationUtils.resourceTypes) {
      let resourceType = MigrationUtils.resourceTypes[resourceTypeKey];
      if (resourceFlags & resourceType) {
        this.recordResourceMigration(extraKeys, resourceType);
      }
    }

    extraKeys.other = String(extraKeys.other);
    this.recordEvent("migration_started", extraKeys);
  },

  observe(aSubject, aTopic, aData) {
    var label;
    switch (aTopic) {
      case "Migration:Started":
        this._succeededMigrationEventArgs = {
          migrator_key: this._source,
          history: "0",
          formdata: "0",
          passwords: "0",
          bookmarks: "0",
          payment_methods: "0",
          // "other" will get incremented, so we keep this as a number for
          // now, and will cast to a string before submitting to Event telemetry.
          other: 0,
        };
        this.recordMigrationStartEvent(this._itemsFlags);
        break;
      case "Migration:ItemBeforeMigrate":
        label = document.getElementById(aData + "_migrated");
        if (label) {
          label.setAttribute("style", "font-weight: bold");
        }
        break;
      case "Migration:ItemAfterMigrate":
        this.recordResourceMigration(this._succeededMigrationEventArgs, aData);
        label = document.getElementById(aData + "_migrated");
        if (label) {
          label.removeAttribute("style");
        }
        break;
      case "Migration:Ended":
        this._succeededMigrationEventArgs.other = String(
          this._succeededMigrationEventArgs.other
        );
        this.recordEvent(
          "migration_finished",
          this._succeededMigrationEventArgs
        );

        if (this.isInitialMigration) {
          // Ensure errors in reporting data recency do not affect the rest of the migration.
          try {
            this.reportDataRecencyTelemetry();
          } catch (ex) {
            console.error(ex);
          }
        }
        if (this._autoMigrate) {
          // We're done now.
          this._wiz.canAdvance = true;
          this._wiz.advance();

          setTimeout(close, 5000);
        } else {
          this._wiz.canAdvance = true;
          var nextButton = this._wiz.getButton("next");
          nextButton.click();
        }
        break;
      case "Migration:ItemError":
        let type = "undefined";
        let numericType = parseInt(aData);
        switch (numericType) {
          case MigrationUtils.resourceTypes.COOKIES:
            type = "cookies";
            break;
          case MigrationUtils.resourceTypes.HISTORY:
            type = "history";
            break;
          case MigrationUtils.resourceTypes.FORMDATA:
            type = "form data";
            break;
          case MigrationUtils.resourceTypes.PASSWORDS:
            type = "passwords";
            break;
          case MigrationUtils.resourceTypes.BOOKMARKS:
            type = "bookmarks";
            break;
          case MigrationUtils.resourceTypes.PAYMENT_METHODS:
            type = "payment methods";
            break;
          case MigrationUtils.resourceTypes.OTHERDATA:
            type = "misc. data";
            break;
        }
        Services.console.logStringMessage(
          "some " + type + " did not successfully migrate."
        );
        Services.telemetry
          .getKeyedHistogramById("FX_MIGRATION_ERRORS")
          .add(this._source, Math.log2(numericType));
        break;
    }
  },

  onDonePageShow() {
    this._wiz.getButton("cancel").disabled = true;
    this._wiz.canRewind = false;
    this._listItems("doneItems");
  },

  reportDataRecencyTelemetry() {
    let histogram = Services.telemetry.getKeyedHistogramById(
      "FX_STARTUP_MIGRATION_DATA_RECENCY"
    );
    let lastUsedPromises = [];
    for (let [key, migrator] of this._availableMigrators) {
      // No block-scoped let in for...of loop conditions, so get the source:
      let localKey = key;
      lastUsedPromises.push(
        migrator.getLastUsedDate().then(date => {
          const ONE_YEAR = 24 * 365;
          let diffInHours = Math.round((Date.now() - date) / (60 * 60 * 1000));
          if (diffInHours > ONE_YEAR) {
            diffInHours = ONE_YEAR;
          }
          histogram.add(localKey, diffInHours);
          return [localKey, diffInHours];
        })
      );
    }
    Promise.all(lastUsedPromises).then(migratorUsedTimeDiff => {
      // Sort low to high.
      migratorUsedTimeDiff.sort(
        ([keyA, diffA], [keyB, diffB]) => diffA - diffB
      ); /* eslint no-unused-vars: off */
      let usedMostRecentBrowser =
        migratorUsedTimeDiff.length &&
        this._source == migratorUsedTimeDiff[0][0];
      let usedRecentBrowser = Services.telemetry.getKeyedHistogramById(
        "FX_STARTUP_MIGRATION_USED_RECENT_BROWSER"
      );
      usedRecentBrowser.add(this._source, usedMostRecentBrowser);
    });
  },
};
