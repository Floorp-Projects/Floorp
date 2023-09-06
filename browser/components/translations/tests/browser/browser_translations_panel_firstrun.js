/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the first run message is displayed.
 */
add_task(async function test_translations_panel_firstrun() {
  const { cleanup, tab } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.panelShown", false]],
  });

  await openTranslationsPanel({ onOpenPanel: assertPanelFirstShowView });

  ok(
    getByL10nId("translations-panel-intro-description"),
    "The intro text is available."
  );

  await clickCancelButton();

  info("Loading a different page.");
  BrowserTestUtils.loadURIString(tab.linkedBrowser, SPANISH_PAGE_URL_2);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });

  info("Checking for the intro text to be hidden.");
  await waitForCondition(
    () => !maybeGetByL10nId("translations-panel-intro-description"),
    "The intro text is no longer shown."
  );

  await clickCancelButton();

  await cleanup();
});
