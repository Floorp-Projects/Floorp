/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the telemetry event for opening the translations panel.
 */
add_task(async function test_translations_telemetry_open_panel() {
  const { cleanup } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedLength: 0,
    }
  );

  const { button } = await assertTranslationsButton(
    { button: true },
    "The button is available."
  );

  await waitForTranslationsPopupEvent("popupshown", () => {
    click(button, "Opening the popup");
  });

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-translate-cancel"),
      "Click the cancel button."
    );
  });

  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedLength: 1,
      finalValuePredicates: [
        value => value.extra.opened_from === "translationsButton",
      ],
    }
  );

  await waitForTranslationsPopupEvent("popupshown", () => {
    click(button, "Opening the popup");
  });

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-translate-cancel"),
      "Click the cancel button."
    );
  });

  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedLength: 2,
      allValuePredicates: [
        value => value.extra.opened_from === "translationsButton",
      ],
    }
  );

  await cleanup();
});
