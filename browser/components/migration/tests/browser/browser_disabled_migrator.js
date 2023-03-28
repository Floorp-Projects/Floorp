/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { InternalTestingProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/InternalTestingProfileMigrator.sys.mjs"
);

add_task(async function test_disabling_migrator() {
  await withMigrationWizardDialog(async prefsWin => {
    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let wizard = dialog.querySelector("migration-wizard");
    let shadow = wizard.openOrClosedShadowRoot;
    let selector = shadow.querySelector("#browser-profile-selector");
    selector.click();

    await new Promise(resolve => {
      wizard
        .querySelector("panel-list")
        .addEventListener("shown", resolve, { once: true });
    });

    let panelItem = wizard.querySelector(
      `panel-item[key="${InternalTestingProfileMigrator.key}"]`
    );
    panelItem.click();

    Assert.ok(
      selector.innerText.includes("Internal Testing Migrator"),
      "Testing for enabled internal testing migrator"
    );
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.migrate.internal-testing.enabled", false]],
  });

  await withMigrationWizardDialog(async prefsWin => {
    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let wizard = dialog.querySelector("migration-wizard");
    let shadow = wizard.openOrClosedShadowRoot;
    let selector = shadow.querySelector("#browser-profile-selector");
    selector.click();

    await new Promise(resolve => {
      wizard
        .querySelector("panel-list")
        .addEventListener("shown", resolve, { once: true });
    });

    let panelItem = wizard.querySelector(
      `panel-item[key="${InternalTestingProfileMigrator.key}"]`
    );

    Assert.ok(!panelItem, "Testing for disabled internal testing migrator");
  });
});
