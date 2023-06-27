/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function hidePopup() {
  return waitForTranslationsPopupEvent("popuphidden", () => {
    info("Hide the popup");
    click(
      getByL10nId("translations-panel-translate-cancel"),
      "Hide the popup."
    );
  });
}

/**
 * Tests that the popup is automatically offered.
 */
add_task(async function test_translations_panel_auto_offer() {
  const panel = document.getElementById("translations-panel");
  const initialPopup = BrowserTestUtils.waitForEvent(panel, "popupshown");

  let popupCount = 0;
  const onPopupShown = () => popupCount++;
  panel.addEventListener("popupshown", onPopupShown);

  const { cleanup, tab } = await loadTestPage({
    page: TRANSLATIONS_TESTER_ES,
    languagePairs: LANGUAGE_PAIRS,
    autoOffer: true,
  });

  info("Waiting for the popup to be shown.");
  await initialPopup;
  is(popupCount, 1, "The popup was opened once.");

  await hidePopup();

  info("Navigate to another page on the same domain.");
  BrowserTestUtils.loadURIString(tab.linkedBrowser, TRANSLATIONS_TESTER_ES_2);

  await assertTranslationsButton(
    { button: true },
    "The button is still shown."
  );
  is(
    popupCount,
    1,
    "The popup was not opened again since the host is the same."
  );

  await waitForTranslationsPopupEvent("popupshown", () => {
    info("Wait for the popup to be shown when navigating to a different host.");
    BrowserTestUtils.loadURIString(
      tab.linkedBrowser,
      TRANSLATIONS_TESTER_ES_DOT_ORG
    );
  });

  is(popupCount, 2, "The popup was only opened twice.");

  await hidePopup();

  panel.removeEventListener("popupshown", onPopupShown);

  await cleanup();
});
