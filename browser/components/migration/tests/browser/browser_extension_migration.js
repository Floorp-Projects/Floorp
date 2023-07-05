/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gFluentStrings = new Localization([
  "branding/brand.ftl",
  "browser/migrationWizard.ftl",
]);

/**
 * Ensures that the wizard is on the progress page and that the extension
 * resource group matches a particular state.
 *
 * @param {Element} wizard
 *   The <migration-wizard> element to inspect.
 * @param {number} state
 *   One of the constants from MigrationWizardConstants.PROGRESS_VALUE,
 *   describing what state the resource group should be in.
 * @param {object} description
 *   An object to express more details of how the resource group should be
 *   displayed.
 * @param {string} description.message
 *   The message that should be displayed for the resource group. This message
 *   maybe be contained in different elements depending on the state.
 * @param {string} description.linkURL
 *   The URL for the <a> element that should be displayed to the user for the
 *   particular state.
 * @param {string} description.linkText
 *   The text content for the <a> element that should be displayed to the user
 *   for the particular state.
 */
function assertExtensionsProgressState(wizard, state, description) {
  let shadow = wizard.openOrClosedShadowRoot;

  // Make sure that we're showing the progress page first.
  let deck = shadow.querySelector("#wizard-deck");
  Assert.equal(
    deck.selectedViewName,
    `page-${MigrationWizardConstants.PAGES.PROGRESS}`
  );

  let progressGroup = shadow.querySelector(
    `.resource-progress-group[data-resource-type="${MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS}"`
  );

  let progressIcon = progressGroup.querySelector(".progress-icon");
  let messageText = progressGroup.querySelector("span.message-text");
  let supportLink = progressGroup.querySelector(".support-text");
  let extensionsSuccessLink = progressGroup.querySelector("a.message-text");

  if (state == MigrationWizardConstants.PROGRESS_VALUE.SUCCESS) {
    Assert.stringMatches(progressIcon.getAttribute("state"), "success");
    Assert.stringMatches(messageText.textContent, "");

    Assert.stringMatches(extensionsSuccessLink.href, "about:addons");
    Assert.stringMatches(
      extensionsSuccessLink.textContent,
      description.message
    );
    Assert.stringMatches(supportLink.textContent, "");
  } else if (state == MigrationWizardConstants.PROGRESS_VALUE.WARNING) {
    Assert.stringMatches(progressIcon.getAttribute("state"), "warning");
    Assert.stringMatches(messageText.textContent, description.message);
    Assert.stringMatches(supportLink.textContent, description.linkText);
    Assert.stringMatches(supportLink.href, description.linkURL);
  } else if (state == MigrationWizardConstants.PROGRESS_VALUE.INFO) {
    Assert.stringMatches(progressIcon.getAttribute("state"), "info");
    Assert.stringMatches(extensionsSuccessLink.href, description.linkURL);
    Assert.stringMatches(
      extensionsSuccessLink.textContent,
      description.message
    );
    Assert.stringMatches(supportLink.textContent, "");
  }
}

/**
 * Checks the case where no extensions were matched.
 */
add_task(async function test_extension_migration_no_matched_extensions() {
  let migration = waitForTestMigration(
    [MigrationUtils.resourceTypes.EXTENSIONS],
    [MigrationUtils.resourceTypes.EXTENSIONS],
    InternalTestingProfileMigrator.testProfile,
    [MigrationUtils.resourceTypes.EXTENSIONS],
    3 /* totalExtensions */,
    0 /* matchedExtensions */
  );

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");

    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );
    selectResourceTypesAndStartMigration(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS,
    ]);
    await migration;
    await wizardDone;
    assertExtensionsProgressState(
      wizard,
      MigrationWizardConstants.PROGRESS_VALUE.WARNING,
      {
        message: await gFluentStrings.formatValue(
          "migration-wizard-progress-no-matched-extensions"
        ),
        linkURL: Services.urlFormatter.formatURLPref(
          "extensions.getAddons.link.url"
        ),
        linkText: await gFluentStrings.formatValue(
          "migration-wizard-progress-extensions-addons-link"
        ),
      }
    );
  });
});

/**
 * Checks the case where some but not all extensions were matched.
 */
add_task(
  async function test_extension_migration_partially_matched_extensions() {
    const TOTAL_EXTENSIONS = 3;
    const TOTAL_MATCHES = 1;

    let migration = waitForTestMigration(
      [MigrationUtils.resourceTypes.EXTENSIONS],
      [MigrationUtils.resourceTypes.EXTENSIONS],
      InternalTestingProfileMigrator.testProfile,
      [],
      TOTAL_EXTENSIONS,
      TOTAL_MATCHES
    );

    await withMigrationWizardDialog(async prefsWin => {
      let dialogBody = prefsWin.document.body;
      let wizard = dialogBody.querySelector("migration-wizard");

      let wizardDone = BrowserTestUtils.waitForEvent(
        wizard,
        "MigrationWizard:DoneMigration"
      );
      selectResourceTypesAndStartMigration(wizard, [
        MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS,
      ]);
      await migration;
      await wizardDone;
      assertExtensionsProgressState(
        wizard,
        MigrationWizardConstants.PROGRESS_VALUE.INFO,
        {
          message: await gFluentStrings.formatValue(
            "migration-wizard-progress-partial-success-extensions",
            {
              matched: TOTAL_MATCHES,
              quantity: TOTAL_EXTENSIONS,
            }
          ),
          linkURL: "about:addons",
          linkText: await gFluentStrings.formatValue(
            "migration-wizard-progress-extensions-support-link"
          ),
        }
      );
    });
  }
);

/**
 * Checks the case where all extensions were matched.
 */
add_task(async function test_extension_migration_fully_matched_extensions() {
  const TOTAL_EXTENSIONS = 15;
  const TOTAL_MATCHES = TOTAL_EXTENSIONS;

  let migration = waitForTestMigration(
    [MigrationUtils.resourceTypes.EXTENSIONS],
    [MigrationUtils.resourceTypes.EXTENSIONS],
    InternalTestingProfileMigrator.testProfile,
    [],
    TOTAL_EXTENSIONS,
    TOTAL_MATCHES
  );

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");

    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );
    selectResourceTypesAndStartMigration(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS,
    ]);
    await migration;
    await wizardDone;
    assertExtensionsProgressState(
      wizard,
      MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
      {
        message: await gFluentStrings.formatValue(
          "migration-wizard-progress-success-extensions",
          {
            quantity: TOTAL_EXTENSIONS,
          }
        ),
        linkURL: "",
        linkText: "",
      }
    );
  });
});
