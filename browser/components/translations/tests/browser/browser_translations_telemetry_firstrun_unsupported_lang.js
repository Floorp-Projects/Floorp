/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the first-interaction event status is maintained across a subsequent panel
 * open, if re-opening the panel is due to requesting to change the source language.
 */
add_task(
  async function test_translations_telemetry_firstrun_unsupported_lang() {
    const { cleanup } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: [
        // Do not include Spanish.
        { fromLang: "fr", toLang: "en" },
        { fromLang: "en", toLang: "fr" },
      ],
      prefs: [["browser.translations.panelShown", false]],
    });

    await openTranslationsPanel({
      openFromAppMenu: true,
      onOpenPanel: assertPanelUnsupportedLanguageView,
    });

    await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
      expectedEventCount: 1,
      expectNewFlowId: true,
      expectFirstInteraction: true,
      finalValuePredicates: [
        value => value.extra.auto_show === "false",
        value => value.extra.view_name === "defaultView",
        value => value.extra.opened_from === "appMenu",
        value => value.extra.document_language === "es",
      ],
    });

    await clickChangeSourceLanguageButton({ firstShow: true });

    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsPanel.changeSourceLanguageButton,
      {
        expectedEventCount: 1,
        expectNewFlowId: false,
        expectFirstInteraction: true,
      }
    );
    await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
      expectedEventCount: 1,
      expectNewFlowId: false,
      expectFirstInteraction: true,
    });
    await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
      expectedEventCount: 2,
      expectNewFlowId: false,
      expectFirstInteraction: true,
      finalValuePredicates: [
        value => value.extra.auto_show === "false",
        value => value.extra.view_name === "defaultView",
        value => value.extra.opened_from === "appMenu",
        value => value.extra.document_language === "es",
      ],
    });

    await clickCancelButton();

    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsPanel.cancelButton,
      {
        expectedEventCount: 1,
        expectNewFlowId: false,
        expectFirstInteraction: true,
      }
    );

    await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
      expectedEventCount: 2,
      expectNewFlowId: false,
      expectFirstInteraction: true,
    });

    await openTranslationsPanel({
      openFromAppMenu: true,
      onOpenPanel: assertPanelUnsupportedLanguageView,
    });

    await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
      expectedEventCount: 3,
      expectNewFlowId: true,
      expectFirstInteraction: false,
      finalValuePredicates: [
        value => value.extra.auto_show === "false",
        value => value.extra.view_name === "defaultView",
        value => value.extra.opened_from === "appMenu",
        value => value.extra.document_language === "es",
      ],
    });

    await clickDismissErrorButton();

    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsPanel.dismissErrorButton,
      {
        expectedEventCount: 1,
        expectNewFlowId: false,
        expectFirstInteraction: false,
      }
    );
    await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
      expectedEventCount: 3,
      expectNewFlowId: false,
      expectFirstInteraction: false,
    });

    await cleanup();
  }
);
