/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests a basic panel open, translation, and restoration to the original language.
 */
add_task(async function test_translations_panel_basics() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  const { button } = await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The button is available."
  );

  is(button.getAttribute("data-l10n-id"), "urlbar-translations-button2");

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

  const panel = document.getElementById("translations-panel");
  const label = document.getElementById(panel.getAttribute("aria-labelledby"));
  ok(label, "The a11y label for the panel can be found.");
  ok(isVisible(label), "The a11y label for the panel is visible.");
  ok(
    getByL10nId("translations-panel-from-label"),
    "The from label is visible."
  );
  ok(
    isVisible(document.getElementById("translations-panel-from")),
    "The from menulist is visible."
  );

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-translate-button"),
      "Start translating by clicking the translate button."
    );
  });

  await assertTranslationsButton(
    { button: true, circleArrows: true, locale: false, icon: true },
    "The icon presents the loading indicator."
  );

  is(button.getAttribute("data-l10n-id"), "urlbar-translations-button-loading");

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(button, "Open the popup again");
    },
    assertPanelDefaultView
  );

  const loadingButton = getByL10nId(
    "translations-panel-translate-button-loading"
  );
  ok(loadingButton, "The loading button is present");
  ok(loadingButton.disabled, "The loading button is disabled");

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-translate-cancel"),
      "Start translating by clicking the translate button."
    );
  });

  await resolveDownloads(1);

  const { locale } = await assertTranslationsButton(
    { button: true, circleArrows: false, locale: true, icon: true },
    "The icon presents the locale."
  );

  is(
    button.getAttribute("data-l10n-id"),
    "urlbar-translations-button-translated"
  );
  is(
    button.getAttribute("data-l10n-args"),
    '{"fromLanguage":"Spanish","toLanguage":"English"}'
  );

  is(locale.innerText, "en", "The English language tag is shown.");

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The pages H1 is translated.",
      getH1,
      "DON QUIJOTE DE LA MANCHA [es to en, html]"
    );
  });

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(button, "Re-opening the popup");
    },
    assertPanelRevisitView
  );

  ok(
    getByL10nId("translations-panel-translate-button").disabled,
    "The translate button is disabled when re-opening."
  );

  ok(
    !maybeGetByL10nId("translations-panel-from-label"),
    "The from label is hidden."
  );
  ok(
    !isVisible(document.getElementById("translations-panel-from")),
    "The from menulist is hidden."
  );

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-restore-button"),
      "Click the restore language button."
    );
  });

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is restored to Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The button is reverted to have an icon."
  );

  await cleanup();
});
