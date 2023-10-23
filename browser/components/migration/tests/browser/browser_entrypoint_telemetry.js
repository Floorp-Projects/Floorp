/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const HISTOGRAM_ID = "FX_MIGRATION_ENTRY_POINT_CATEGORICAL";

async function showThenCloseMigrationWizardViaEntrypoint(entrypoint) {
  let openedPromise = BrowserTestUtils.waitForMigrationWizard(window);

  MigrationUtils.showMigrationWizard(window, {
    entrypoint,
  });

  let wizardTab = await openedPromise;
  Assert.ok(wizardTab, "Migration wizard opened.");

  await BrowserTestUtils.removeTab(wizardTab);
}

add_setup(async () => {
  // Load the initial tab at example.com. This makes it so that if
  // when we load the wizard in about:preferences, we'll load the
  // about:preferences page in a new tab rather than overtaking the
  // initial one. This makes cleanup of the wizard more explicit, since
  // we can just close the tab.
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "https://example.com");
  await BrowserTestUtils.browserLoaded(browser);
});

/**
 * Tests that the entrypoint passed to MigrationUtils.showMigrationWizard gets
 * written to both the FX_MIGRATION_ENTRY_POINT_CATEGORICAL histogram.
 */
add_task(async function test_entrypoints() {
  let histogram = TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_ID);

  // Let's arbitrarily pick the "Bookmarks" entrypoint, and make sure this
  // is recorded.
  await showThenCloseMigrationWizardViaEntrypoint(
    MigrationUtils.MIGRATION_ENTRYPOINTS.BOOKMARKS
  );
  let entrypointIndex = getEntrypointHistogramIndex(
    MigrationUtils.MIGRATION_ENTRYPOINTS.BOOKMARKS
  );

  TelemetryTestUtils.assertHistogram(histogram, entrypointIndex, 1);

  histogram = TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_ID);

  // Now let's pick the "Preferences" entrypoint, and make sure this
  // is recorded.
  await showThenCloseMigrationWizardViaEntrypoint(
    MigrationUtils.MIGRATION_ENTRYPOINTS.PREFERENCES
  );
  entrypointIndex = getEntrypointHistogramIndex(
    MigrationUtils.MIGRATION_ENTRYPOINTS.PREFERENCES
  );

  TelemetryTestUtils.assertHistogram(histogram, entrypointIndex, 1);

  histogram = TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_ID);

  // Finally, check the fallback by passing in something invalid as an entrypoint.
  await showThenCloseMigrationWizardViaEntrypoint(undefined);
  entrypointIndex = getEntrypointHistogramIndex(
    MigrationUtils.MIGRATION_ENTRYPOINTS.UNKNOWN
  );

  TelemetryTestUtils.assertHistogram(histogram, entrypointIndex, 1);
});
