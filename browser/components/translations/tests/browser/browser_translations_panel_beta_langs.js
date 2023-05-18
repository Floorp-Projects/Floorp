/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that languages are displayed correctly as being in beta or not.
 */
add_task(async function test_translations_panel_display_beta_languages() {
  const { cleanup } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  function assertBetaDisplay(selectElement) {
    const betaL10nId = "translations-panel-displayname-beta";
    const options = selectElement.firstChild.getElementsByTagName("menuitem");
    for (const option of options) {
      for (const languagePair of LANGUAGE_PAIRS) {
        if (
          languagePair.fromLang === option.value ||
          languagePair.toLang === option.value
        ) {
          if (option.getAttribute("data-l10n-id") === betaL10nId) {
            is(
              languagePair.isBeta,
              true,
              `Since data-l10n-id was ${betaL10nId} for ${option.value}, then it must be part of a beta language pair, but it was not.`
            );
          }
          if (!languagePair.isBeta) {
            is(
              option.getAttribute("data-l10n-id") === betaL10nId,
              false,
              `Since the languagePair is non-beta, the language option ${option.value} should not have a data-l10-id of ${betaL10nId}, but it does.`
            );
          }
        }
      }
    }
  }

  const fromSelect = document.getElementById("translations-panel-dual-from");
  const toSelect = document.getElementById("translations-panel-dual-to");

  assertBetaDisplay(fromSelect);
  assertBetaDisplay(toSelect);

  await cleanup();
});
