/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests how the unsupported language flow works.
 */
add_task(async function test_unsupported_lang() {
  info("Start at a page in Spanish.");
  const { cleanup } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: [
      // Do not include Spanish.
      { fromLang: "fr", toLang: "en", isBeta: false },
      { fromLang: "en", toLang: "fr", isBeta: false },
    ],
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

  ok(
    getByL10nId("translations-panel-error-unsupported"),
    "The unsupported title is shown."
  );
  ok(
    getByL10nId("translations-panel-error-unsupported-hint-known"),
    "The unsupported language message is shown."
  );
  ok(
    getByL10nId("translations-panel-learn-more-link"),
    "The learn more link is available"
  );

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(
        getByL10nId("translations-panel-error-change-button"),
        "Change the languages."
      );
    },
    assertPanelDefaultView
  );

  info("Waiting to find the translations panel header.");
  const header = await waitForCondition(() =>
    maybeGetByL10nId("translations-panel-header")
  );
  ok(
    header,
    "The original panel header is there and we are ready to translate again."
  );
  ok(
    !maybeGetByL10nId("translations-panel-error-unsupported"),
    "The unsupported title is hidden."
  );
  await cleanup();
});
