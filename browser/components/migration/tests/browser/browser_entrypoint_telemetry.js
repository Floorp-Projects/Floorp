/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
});
const CONTENT_MODAL_ENABLED_PREF = "browser.migrate.content-modal.enabled";
const HISTOGRAM_ID = "FX_MIGRATION_ENTRY_POINT_CATEGORICAL";
const LEGACY_HISTOGRAM_ID = "FX_MIGRATION_ENTRY_POINT";

async function showThenCloseMigrationWizardViaEntrypoint(entrypoint) {
  const LEGACY_DIALOG = !Services.prefs.getBoolPref(CONTENT_MODAL_ENABLED_PREF);
  let openedPromise = LEGACY_DIALOG
    ? BrowserTestUtils.domWindowOpenedAndLoaded(null, win => {
        let type = win.document.documentElement.getAttribute("windowtype");
        if (type == "Browser:MigrationWizard") {
          Assert.ok(true, "Saw legacy Migration Wizard window open.");
          return true;
        }

        return false;
      })
    : waitForMigrationWizardDialogTab();

  // On some platforms, this call blocks, so in order to let the test proceed, we
  // run it on the next tick of the event loop.
  executeSoon(() => {
    MigrationUtils.showMigrationWizard(window, {
      entrypoint,
    });
  });

  let result = await openedPromise;
  if (LEGACY_DIALOG) {
    await BrowserTestUtils.closeWindow(result);
  } else if (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.getTabForBrowser(result));
  } else {
    BrowserTestUtils.loadURIString(result, "about:blank");
    await BrowserTestUtils.browserLoaded(result);
  }
}

/**
 * Tests that the entrypoint passed to MigrationUtils.showMigrationWizard gets
 * written to both the FX_MIGRATION_ENTRY_POINT_CATEGORICAL histogram, as well
 * as the legacy FX_MIGRATION_ENTRY_POINT histogram (but only if using the old
 * wizard window).
 */
add_task(async function test_legacy_wizard() {
  for (let contentModalEnabled of [true, false]) {
    info("Testing with content modal enabled: " + contentModalEnabled);
    await SpecialPowers.pushPrefEnv({
      set: [[CONTENT_MODAL_ENABLED_PREF, contentModalEnabled]],
    });

    let histogram = TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_ID);
    let legacyHistogram = TelemetryTestUtils.getAndClearHistogram(
      LEGACY_HISTOGRAM_ID
    );

    // Let's arbitrarily pick the "Bookmarks" entrypoint, and make sure this
    // is recorded.
    await showThenCloseMigrationWizardViaEntrypoint(
      MigrationUtils.MIGRATION_ENTRYPOINTS.BOOKMARKS
    );
    let entrypointId = MigrationUtils.getLegacyMigrationEntrypoint(
      MigrationUtils.MIGRATION_ENTRYPOINTS.BOOKMARKS
    );

    TelemetryTestUtils.assertHistogram(histogram, entrypointId, 1);

    if (!contentModalEnabled) {
      TelemetryTestUtils.assertHistogram(legacyHistogram, entrypointId, 1);
    }

    histogram = TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_ID);
    legacyHistogram = TelemetryTestUtils.getAndClearHistogram(
      LEGACY_HISTOGRAM_ID
    );

    // Now let's pick the "Preferences" entrypoint, and make sure this
    // is recorded.
    await showThenCloseMigrationWizardViaEntrypoint(
      MigrationUtils.MIGRATION_ENTRYPOINTS.PREFERENCES
    );
    entrypointId = MigrationUtils.getLegacyMigrationEntrypoint(
      MigrationUtils.MIGRATION_ENTRYPOINTS.PREFERENCES
    );

    TelemetryTestUtils.assertHistogram(histogram, entrypointId, 1);
    if (!contentModalEnabled) {
      TelemetryTestUtils.assertHistogram(legacyHistogram, entrypointId, 1);
    }

    histogram = TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_ID);
    legacyHistogram = TelemetryTestUtils.getAndClearHistogram(
      LEGACY_HISTOGRAM_ID
    );

    // Finally, check the fallback by passing in something invalid as an entrypoint.
    await showThenCloseMigrationWizardViaEntrypoint(undefined);
    entrypointId = MigrationUtils.getLegacyMigrationEntrypoint(
      MigrationUtils.MIGRATION_ENTRYPOINTS.UNKNOWN
    );

    TelemetryTestUtils.assertHistogram(histogram, entrypointId, 1);
    if (!contentModalEnabled) {
      TelemetryTestUtils.assertHistogram(legacyHistogram, entrypointId, 1);
    }
  }
});
