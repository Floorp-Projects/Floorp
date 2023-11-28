/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const IMPORT_SCREEN = {
  id: "AW_IMPORT",
  content: {
    primary_button: {
      label: "import",
      action: {
        navigate: true,
        type: "SHOW_MIGRATION_WIZARD",
      },
    },
  },
};

add_task(async function test_wait_import_modal() {
  await setAboutWelcomeMultiStage(
    JSON.stringify([IMPORT_SCREEN, { id: "AW_NEXT", content: {} }])
  );
  const { cleanup, browser } = await openMRAboutWelcome();

  // execution
  await test_screen_content(
    browser,
    "renders IMPORT screen",
    //Expected selectors
    ["main.AW_IMPORT", "button[value='primary_button']"],

    //Unexpected selectors:
    ["main.AW_NEXT"]
  );

  const wizardPromise = BrowserTestUtils.waitForMigrationWizard(window);
  const prefsTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );
  await onButtonClick(browser, "button.primary");
  const wizard = await wizardPromise;

  await test_screen_content(
    browser,
    "still shows IMPORT screen",
    //Expected selectors
    ["main.AW_IMPORT", "button[value='primary_button']"],

    //Unexpected selectors:
    ["main.AW_NEXT"]
  );

  await BrowserTestUtils.removeTab(wizard);

  await test_screen_content(
    browser,
    "moved to NEXT screen",
    //Expected selectors
    ["main.AW_NEXT"],

    //Unexpected selectors:
    []
  );

  // cleanup
  await SpecialPowers.popPrefEnv(); // for setAboutWelcomeMultiStage
  BrowserTestUtils.removeTab(prefsTab);
  await cleanup();
});

add_task(async function test_wait_import_spotlight() {
  const spotlightPromise = TestUtils.topicObserved("subdialog-loaded");
  ChromeUtils.import(
    "resource://activity-stream/lib/Spotlight.jsm"
  ).Spotlight.showSpotlightDialog(gBrowser.selectedBrowser, {
    content: { modal: "tab", screens: [IMPORT_SCREEN] },
  });
  const [win] = await spotlightPromise;

  const wizardPromise = BrowserTestUtils.waitForMigrationWizard(window);
  const prefsTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );
  win.document
    .querySelector(".onboardingContainer button[value='primary_button']")
    .click();
  const wizard = await wizardPromise;

  await BrowserTestUtils.removeTab(wizard);

  // cleanup
  BrowserTestUtils.removeTab(prefsTab);
});
