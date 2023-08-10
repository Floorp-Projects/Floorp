/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the telemetry events for switching the from-language.
 */
add_task(async function test_translations_telemetry_switch_from_language() {
  const { cleanup, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  const { button } = await assertTranslationsButton(
    { button: true },
    "The button is available."
  );

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(button, "Opening the popup");
    },
    assertPanelDefaultView
  );

  const fromSelect = getById("translations-panel-from");

  is(fromSelect.value, "es", "The from select starts as Spanish");

  info('Switch from language to "es"');
  fromSelect.value = "en";
  fromSelect.dispatchEvent(new Event("command"));

  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedEventCount: 1,
      expectNewFlowId: true,
      finalValuePredicates: [
        value => value.extra.auto_show === "false",
        value => value.extra.view_name === "defaultView",
        value => value.extra.opened_from === "translationsButton",
        value => value.extra.document_language === "es",
      ],
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "ChangeFromLanguage",
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "en"],
    }
  );

  info('Switch from language back to "es"');
  fromSelect.value = "es";
  fromSelect.dispatchEvent(new Event("command"));

  await TestTranslationsTelemetry.assertEvent(
    "ChangeFromLanguage",
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 2,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "es"],
    }
  );

  info("Switch to language to nothing");
  fromSelect.value = "";
  fromSelect.dispatchEvent(new Event("command"));

  await TestTranslationsTelemetry.assertEvent(
    "ChangeFromLanguage",
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 2,
    }
  );

  info('Switch from language to "en"');
  fromSelect.value = "en";
  fromSelect.dispatchEvent(new Event("command"));

  await TestTranslationsTelemetry.assertEvent(
    "ChangeFromLanguage",
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 3,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "en"],
    }
  );

  await cleanup();
});

/**
 * Tests the telemetry events for switching the to-language.
 */
add_task(async function test_translations_telemetry_switch_to_language() {
  const { cleanup, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  const { button } = await assertTranslationsButton(
    { button: true },
    "The button is available."
  );

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(button, "Opening the popup");
    },
    assertPanelDefaultView
  );

  const toSelect = getById("translations-panel-to");

  is(toSelect.value, "en", "The to select starts as English");

  info('Switch to language to "fr"');
  toSelect.value = "fr";
  toSelect.dispatchEvent(new Event("command"));

  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedEventCount: 1,
      expectNewFlowId: true,
      finalValuePredicates: [
        value => value.extra.auto_show === "false",
        value => value.extra.view_name === "defaultView",
        value => value.extra.opened_from === "translationsButton",
        value => value.extra.document_language === "es",
      ],
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "ChangeToLanguage",
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "fr"],
    }
  );

  info('Switch to language back to "en"');
  toSelect.value = "en";
  toSelect.dispatchEvent(new Event("command"));

  await TestTranslationsTelemetry.assertEvent(
    "ChangeToLanguage",
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 2,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "en"],
    }
  );

  info("Switch to language to nothing");
  toSelect.value = "";
  toSelect.dispatchEvent(new Event("command"));

  await TestTranslationsTelemetry.assertEvent(
    "ChangeToLanguage",
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 2,
    }
  );

  info('Switch to language to "en"');
  toSelect.value = "en";
  toSelect.dispatchEvent(new Event("command"));

  await TestTranslationsTelemetry.assertEvent(
    "ChangeToLanguage",
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 3,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "en"],
    }
  );

  await cleanup();
});
