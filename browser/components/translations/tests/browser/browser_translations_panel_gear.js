/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test managing the languages menu item.
 */
add_task(async function test_translations_panel_manage_languages() {
  const { cleanup } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  const { button } = await assertTranslationsButton(
    { button: true },
    "The button is available."
  );

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(button, "Opening the popup");
    },
    assertPanelDefaultView
  );

  const gearIcon = getByL10nId("translations-panel-settings-button");
  click(gearIcon, "Open the preferences menu");

  ok(
    getByL10nId("translations-panel-settings-about2"),
    "The learn more link is in the gear menu."
  );
  // Aside: Don't actually click the link in the test since it will trigger a
  // network request.

  const manageLanguages = getByL10nId(
    "translations-panel-settings-manage-languages"
  );
  info("Choose to manage the languages.");
  manageLanguages.doCommand();

  await waitForCondition(
    () => gBrowser.currentURI.spec === "about:preferences#general",
    "Waiting for about:preferences to be opened."
  );

  info("Remove the about:preferences tab");
  gBrowser.removeCurrentTab();

  await cleanup();
});
