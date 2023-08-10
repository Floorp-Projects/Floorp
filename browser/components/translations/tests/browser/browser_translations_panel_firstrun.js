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

  const { button } = await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The button is available."
  );

  is(
    button.getAttribute("data-l10n-id"),
    "urlbar-translations-button-intro",
    "The intro message is displayed."
  );

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(button, "Opening the popup");
    },
    assertPanelFirstShowView
  );

  ok(
    getByL10nId("translations-panel-intro-description"),
    "The intro text is available."
  );

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-translate-cancel"),
      "Dismiss the panel"
    );
  });

  info("Loading a different page.");
  BrowserTestUtils.loadURIString(tab.linkedBrowser, SPANISH_PAGE_URL_2);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  is(
    button.getAttribute("data-l10n-id"),
    "urlbar-translations-button2",
    "The intro message is no longer there."
  );

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(button, "Opening the popup");
    },
    assertPanelDefaultView
  );

  info("Checking for the intro text to be hidden.");
  await waitForCondition(
    () => !maybeGetByL10nId("translations-panel-intro-description"),
    "The intro text is no longer shown."
  );

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-translate-cancel"),
      "Dismiss the panel"
    );
  });

  await cleanup();
});
