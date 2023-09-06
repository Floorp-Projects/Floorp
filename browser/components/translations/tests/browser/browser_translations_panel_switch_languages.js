/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests switching the language.
 */
add_task(async function test_translations_panel_switch_language() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  await assertTranslationsButton({ button: true }, "The button is available.");

  await assertPageIsUntranslated(runInPage);

  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });

  const { translateButton, fromMenuList, toMenuList } =
    TranslationsPanel.elements;

  ok(!translateButton.disabled, "The translate button starts as enabled");
  is(fromMenuList.value, "es", "The from select starts as Spanish");
  is(toMenuList.value, "en", "The to select starts as English");

  info('Switch from language to "es"');
  fromMenuList.value = "en";
  fromMenuList.dispatchEvent(new Event("command"));

  ok(
    translateButton.disabled,
    "The translate button is disabled when the languages are the same"
  );

  info('Switch from language back to "es"');
  fromMenuList.value = "es";
  fromMenuList.dispatchEvent(new Event("command"));

  ok(
    !translateButton.disabled,
    "When the languages are different it can be translated"
  );

  info("Switch to language to nothing");
  fromMenuList.value = "";
  fromMenuList.dispatchEvent(new Event("command"));

  ok(
    translateButton.disabled,
    "The translate button is disabled nothing is selected."
  );

  info('Switch from language to "en"');
  fromMenuList.value = "en";
  fromMenuList.dispatchEvent(new Event("command"));

  info('Switch to language to "fr"');
  toMenuList.value = "fr";
  toMenuList.dispatchEvent(new Event("command"));

  ok(!translateButton.disabled, "The translate button can now be used");

  await clickTranslateButton({
    downloadHandler: resolveDownloads,
  });

  await assertPageIsTranslated("en", "fr", runInPage);

  await cleanup();
});
