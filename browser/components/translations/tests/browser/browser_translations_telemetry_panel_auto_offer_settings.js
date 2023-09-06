/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the automatic offering of the popup can be disabled.
 */
add_task(async function test_translations_panel_auto_offer_settings() {
  const { cleanup } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    // Use the auto offer mechanics, but default the pref to the off position.
    autoOffer: true,
    prefs: [["browser.translations.automaticallyPopup", false]],
  });

  await assertTranslationsButton(
    { button: true },
    "The translations button is shown."
  );

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 0,
  });

  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });
  await openTranslationsSettingsMenu();
  await assertIsAlwaysOfferTranslationsEnabled(false);

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 1,
    expectNewFlowId: true,
    allValuePredicates: [
      value => value.extra.auto_show === "false",
      value => value.extra.view_name === "defaultView",
      value => value.extra.opened_from === "translationsButton",
      value => value.extra.document_language === "es",
    ],
  });

  await clickAlwaysOfferTranslations();

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.alwaysOfferTranslations,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      allValuePredicates: [value => value.extra.toggled_on === "true"],
    }
  );

  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });
  await assertIsAlwaysOfferTranslationsEnabled(true);

  await clickCancelButton();

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 2,
    expectNewFlowId: true,
    allValuePredicates: [
      value => value.extra.auto_show === "false",
      value => value.extra.view_name === "defaultView",
      value => value.extra.opened_from === "translationsButton",
      value => value.extra.document_language === "es",
    ],
  });

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.cancelButton,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
    }
  );

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 2,
    expectNewFlowId: false,
  });

  await navigate(
    "Wait for the popup to be shown when navigating to a different host.",
    { url: SPANISH_PAGE_URL_DOT_ORG, onOpenPanel: assertPanelDefaultView }
  );

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 3,
    expectNewFlowId: true,
    finalValuePredicates: [
      value => value.extra.auto_show === "true",
      value => value.extra.view_name === "defaultView",
      value => value.extra.opened_from === "translationsButton",
      value => value.extra.document_language === "es",
    ],
  });

  await cleanup();
});
