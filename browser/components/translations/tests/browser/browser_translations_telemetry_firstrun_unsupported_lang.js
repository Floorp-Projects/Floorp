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

    const appMenuButton = getById("PanelUI-menu-button");

    click(appMenuButton, "Opening the app menu");
    await BrowserTestUtils.waitForEvent(window.PanelUI.mainView, "ViewShown");

    await waitForTranslationsPopupEvent(
      "popupshown",
      () => {
        click(getByL10nId("appmenuitem-translate"), "Opening the popup");
      },
      assertPanelUnsupportedLanguageView
    );

    await TestTranslationsTelemetry.assertEvent(
      "OpenPanel",
      Glean.translationsPanel.open,
      {
        expectedEventCount: 1,
        expectNewFlowId: true,
        expectFirstInteraction: true,
        finalValuePredicates: [
          value => value.extra.auto_show === "false",
          value => value.extra.view_name === "defaultView",
          value => value.extra.opened_from === "appMenu",
          value => value.extra.document_language === "es",
        ],
      }
    );

    ok(
      getByL10nId("translations-panel-error-unsupported"),
      "The unsupported title is shown."
    );

    await waitForTranslationsPopupEvent(
      "popupshown",
      () => {
        click(
          getByL10nId("translations-panel-error-change-button"),
          "Change the languages."
        );
      },
      assertPanelFirstShowView
    );

    info("Waiting to find the translations panel header.");
    const header = await waitForCondition(() =>
      maybeGetByL10nId("translations-panel-intro-description")
    );

    ok(
      header,
      "The first-run panel header is there and we are ready to continue"
    );
    ok(
      !maybeGetByL10nId("translations-panel-error-unsupported"),
      "The unsupported title is hidden."
    );

    await TestTranslationsTelemetry.assertEvent(
      "ChangeSourceLanguageButton",
      Glean.translationsPanel.changeSourceLanguageButton,
      {
        expectedEventCount: 1,
        expectNewFlowId: false,
        expectFirstInteraction: true,
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      "ClosePanel",
      Glean.translationsPanel.close,
      {
        expectedEventCount: 1,
        expectNewFlowId: false,
        expectFirstInteraction: true,
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      "OpenPanel",
      Glean.translationsPanel.open,
      {
        expectedEventCount: 2,
        expectNewFlowId: false,
        expectFirstInteraction: true,
        finalValuePredicates: [
          value => value.extra.auto_show === "false",
          value => value.extra.view_name === "defaultView",
          value => value.extra.opened_from === "appMenu",
          value => value.extra.document_language === "es",
        ],
      }
    );

    await waitForTranslationsPopupEvent("popuphidden", () => {
      click(
        getByL10nId("translations-panel-translate-cancel"),
        "Click the cancel button."
      );
    });

    await TestTranslationsTelemetry.assertEvent(
      "CancelButton",
      Glean.translationsPanel.cancelButton,
      {
        expectedEventCount: 1,
        expectNewFlowId: false,
        expectFirstInteraction: true,
      }
    );

    await TestTranslationsTelemetry.assertEvent(
      "ClosePanel",
      Glean.translationsPanel.close,
      {
        expectedEventCount: 2,
        expectNewFlowId: false,
        expectFirstInteraction: true,
      }
    );

    click(appMenuButton, "Opening the app menu");
    await BrowserTestUtils.waitForEvent(window.PanelUI.mainView, "ViewShown");

    await waitForTranslationsPopupEvent(
      "popupshown",
      () => {
        click(getByL10nId("appmenuitem-translate"), "Opening the popup");
      },
      assertPanelUnsupportedLanguageView
    );

    await TestTranslationsTelemetry.assertEvent(
      "OpenPanel",
      Glean.translationsPanel.open,
      {
        expectedEventCount: 3,
        expectNewFlowId: true,
        expectFirstInteraction: false,
        finalValuePredicates: [
          value => value.extra.auto_show === "false",
          value => value.extra.view_name === "defaultView",
          value => value.extra.opened_from === "appMenu",
          value => value.extra.document_language === "es",
        ],
      }
    );

    await waitForTranslationsPopupEvent("popuphidden", () => {
      click(
        getByL10nId("translations-panel-error-dismiss-button"),
        "Click the dismiss error button."
      );
    });

    await TestTranslationsTelemetry.assertEvent(
      "DismissErrorButton",
      Glean.translationsPanel.dismissErrorButton,
      {
        expectedEventCount: 1,
        expectNewFlowId: false,
        expectFirstInteraction: false,
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      "ClosePanel",
      Glean.translationsPanel.close,
      {
        expectedEventCount: 3,
        expectNewFlowId: false,
        expectFirstInteraction: false,
      }
    );

    await cleanup();
  }
);
