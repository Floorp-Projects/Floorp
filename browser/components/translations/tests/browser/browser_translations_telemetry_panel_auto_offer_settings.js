/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the automatic offering of the popup can be disabled.
 */
add_task(async function test_translations_panel_auto_offer_settings() {
  info("Load the test page in English so that no popups will be offered.");
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

  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedEventCount: 0,
    }
  );

  info("Open the popup and gear icon menu.");
  const alwaysOfferId = "translations-panel-settings-always-offer-translation";

  await openTranslationsSettingsMenuViaTranslationsButton();
  await assertCheckboxState(alwaysOfferId, { checked: false });

  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedEventCount: 1,
      expectNewFlowId: true,
      allValuePredicates: [
        value => value.extra.auto_show === "false",
        value => value.extra.view_name === "defaultView",
        value => value.extra.opened_from === "translationsButton",
        value => value.extra.document_language === "es",
      ],
    }
  );

  info("Turn on automatic offering of popups");
  await clickAlwaysOfferTranslations();

  await TestTranslationsTelemetry.assertEvent(
    "AlwaysOfferTranslations",
    Glean.translationsPanel.alwaysOfferTranslations,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      allValuePredicates: [value => value.extra.toggled_on === "true"],
    }
  );

  await openTranslationsSettingsMenuViaTranslationsButton();
  await assertCheckboxState(alwaysOfferId, { checked: true });

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
      expectedEventCount: 2,
      expectNewFlowId: true,
      allValuePredicates: [
        value => value.extra.auto_show === "false",
        value => value.extra.view_name === "defaultView",
        value => value.extra.opened_from === "translationsButton",
        value => value.extra.document_language === "es",
      ],
    }
  );

  await TestTranslationsTelemetry.assertEvent(
    "CancelButton",
    Glean.translationsPanel.cancelButton,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
    }
  );

  await TestTranslationsTelemetry.assertEvent(
    "ClosePanel",
    Glean.translationsPanel.close,
    {
      expectedEventCount: 2,
      expectNewFlowId: false,
    }
  );

  await waitForTranslationsPopupEvent(
    "popupshown",
    async () => {
      await navigate(
        SPANISH_PAGE_URL_DOT_ORG,
        "Wait for the popup to be shown when navigating to a different host."
      );
    },
    assertPanelDefaultView
  );

  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedEventCount: 3,
      expectNewFlowId: true,
      finalValuePredicates: [
        value => value.extra.auto_show === "true",
        value => value.extra.view_name === "defaultView",
        value => value.extra.opened_from === "translationsButton",
        value => value.extra.document_language === "es",
      ],
    }
  );

  await cleanup();
});
