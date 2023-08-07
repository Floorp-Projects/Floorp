/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This tests a specific defect where the revisit view was not showing properly
 * when navigating to an auto-translated page after visiting a page in an unsupported
 * language and viewing the panel.
 *
 * This test case tests the case where the auto translate succeeds and the user
 * manually opens the panel to show the revisit view.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1845611 for more information.
 */
add_task(
  async function test_revisit_view_updates_with_auto_translate_success() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: [
        // Do not include French.
        { fromLang: "en", toLang: "es" },
        { fromLang: "es", toLang: "en" },
      ],
      prefs: [["browser.translations.alwaysTranslateLanguages", ""]],
    });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The translations button is visible."
    );

    info(
      "Simulate clicking always-translate-language in the settings menu, " +
        "adding the document language to the alwaysTranslateLanguages pref"
    );
    await openTranslationsSettingsMenuViaTranslationsButton();

    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await toggleAlwaysTranslateLanguage();
    await assertIsAlwaysTranslateLanguage("es", { checked: true });

    await assertTranslationsButton(
      { button: true, circleArrows: true, locale: false, icon: true },
      "The icon presents the loading indicator."
    );

    await resolveDownloads(1);

    const { locale } = await assertTranslationsButton(
      { button: true, circleArrows: false, locale: true, icon: true },
      "The icon presents the locale."
    );

    is(locale.innerText, "en", "The English language tag is shown.");

    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is translated automatically",
        getH1,
        "DON QUIJOTE DE LA MANCHA [es to en, html]"
      );
    });

    info("Navigate to a page in an unsupported language");
    await navigate(FRENCH_PAGE_URL);

    await assertTranslationsButton(
      { button: false },
      "The translations button should be unavailable."
    );

    info(
      "Open the translations panel to show the default unsupported language view."
    );
    await openTranslationsPanelViaAppMenu();

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

    info("Navigate back to the spanish page.");
    await navigate(SPANISH_PAGE_URL_DOT_ORG);

    await assertTranslationsButton(
      { button: true, circleArrows: true, locale: false, icon: true },
      "The icon presents the loading indicator."
    );

    await resolveDownloads(1);

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: true, icon: true },
      "The icon presents the locale."
    );

    is(locale.innerText, "en", "The English language tag is shown.");

    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is translated automatically",
        getH1,
        "DON QUIJOTE DE LA MANCHA [es to en, html]"
      );
    });

    await openTranslationsPanelViaAppMenu();

    info("Waiting to find the translations panel revisit header.");
    const header = await waitForCondition(() =>
      maybeGetByL10nId("translations-panel-revisit-header")
    );
    ok(header, "The revisit panel header is there.");
    ok(
      !maybeGetByL10nId("translations-panel-error-unsupported"),
      "The unsupported header is hidden."
    );

    await cleanup();
  }
);
