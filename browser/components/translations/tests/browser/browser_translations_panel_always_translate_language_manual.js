/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the effect of toggling the always-translate-language menuitem after the page has
 * been manually translated. This should not reload or retranslate the page, but just check
 * the box.
 */
add_task(
  async function test_activate_always_translate_language_after_manual_translation() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
    });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
    );

    await assertPageIsUntranslated(runInPage);

    await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });

    await clickTranslateButton({
      downloadHandler: resolveDownloads,
    });

    await assertPageIsTranslated("es", "en", runInPage);

    await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await clickAlwaysTranslateLanguage();
    await assertIsAlwaysTranslateLanguage("es", { checked: true });

    await assertPageIsTranslated("es", "en", runInPage);

    await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await clickAlwaysTranslateLanguage();
    await assertIsAlwaysTranslateLanguage("es", { checked: false });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "Only the button appears"
    );

    await assertPageIsUntranslated(runInPage);

    await cleanup();
  }
);
