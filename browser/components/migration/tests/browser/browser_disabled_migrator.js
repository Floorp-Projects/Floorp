/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MigratorBase } = ChromeUtils.importESModule(
  "resource:///modules/MigratorBase.sys.mjs"
);

/**
 * Tests that the InternalTestingProfileMigrator is listed in
 * the new migration wizard selector when enabled.
 */
add_task(async function test_enabled_migrator() {
  await withMigrationWizardDialog(async prefsWin => {
    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let wizard = dialog.querySelector("migration-wizard");
    let shadow = wizard.openOrClosedShadowRoot;
    let selector = shadow.querySelector("#browser-profile-selector");
    selector.click();

    await new Promise(resolve => {
      shadow
        .querySelector("panel-list")
        .addEventListener("shown", resolve, { once: true });
    });

    let panelItem = shadow.querySelector(
      `panel-item[key="${InternalTestingProfileMigrator.key}"]`
    );

    Assert.ok(
      panelItem,
      "The InternalTestingProfileMigrator panel-item exists."
    );
    panelItem.click();

    Assert.ok(
      selector.innerText.includes("Internal Testing Migrator"),
      "Testing for enabled internal testing migrator"
    );
  });
});

/**
 * Tests that the InternalTestingProfileMigrator is not listed in
 * the new migration wizard selector when disabled.
 */
add_task(async function test_disabling_migrator() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.migrate.internal-testing.enabled", false]],
  });

  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  let internalTestingMigrator = new InternalTestingProfileMigrator();

  // We create a fake migrator that we know will still be present after
  // disabling the InternalTestingProfileMigrator so that we don't switch
  // the wizard to the NO_BROWSERS_FOUND page, which we're not testing here.
  let fakeMigrator = new FakeMigrator();

  let getMigratorStub = sandbox.stub(MigrationUtils, "getMigrator");
  getMigratorStub
    .withArgs("internal-testing")
    .resolves(internalTestingMigrator);
  getMigratorStub.withArgs("fake-migrator").resolves(fakeMigrator);

  sandbox.stub(MigrationUtils, "availableMigratorKeys").get(() => {
    return ["internal-testing", "fake-migrator"];
  });

  await withMigrationWizardDialog(async prefsWin => {
    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let wizard = dialog.querySelector("migration-wizard");
    let shadow = wizard.openOrClosedShadowRoot;
    let selector = shadow.querySelector("#browser-profile-selector");
    selector.click();

    await new Promise(resolve => {
      shadow
        .querySelector("panel-list")
        .addEventListener("shown", resolve, { once: true });
    });

    let panelItem = shadow.querySelector(
      `panel-item[key="${InternalTestingProfileMigrator.key}"]`
    );

    Assert.ok(
      !panelItem,
      "The panel-item for the InternalTestingProfileMigrator does not exist"
    );
  });

  sandbox.restore();
});

/**
 * A stub of a migrator used for automated testing only.
 */
class FakeMigrator extends MigratorBase {
  static get key() {
    return "fake-migrator";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-firefox";
  }

  // We will create a single MigratorResource for each resource type that
  // just immediately reports a successful migration.
  getResources() {
    return Object.values(MigrationUtils.resourceTypes).map(type => {
      return {
        type,
        migrate: callback => {
          callback(true /* success */);
        },
      };
    });
  }

  // We need to override enabled() to always return true for testing purposes.
  get enabled() {
    return true;
  }
}
