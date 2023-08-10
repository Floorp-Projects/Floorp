/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the first-show intro message message is displayed
 * when viewing the panel subsequent times on the same URL,
 * but is no longer displayed after navigating to new URL,
 * or when returning to the first URL after navigating away.
 */
add_task(async function test_translations_panel_firstrun() {
  const { cleanup } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.panelShown", false]],
  });

  const { button } = await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The button is available."
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

  await navigate(SPANISH_PAGE_URL_DOT_ORG, "Navigate to a different website");

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

  await navigate(SPANISH_PAGE_URL, "Navigate back to the first website");

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
