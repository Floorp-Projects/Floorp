"use strict";

const SEPARATE_ABOUT_WELCOME_PREF = "browser.aboutwelcome.enabled";

/**
 * Sets the aboutwelcome pref to enabled simplified welcome UI
 */
async function setAboutWelcomePref(value) {
  return pushPrefs([SEPARATE_ABOUT_WELCOME_PREF, value]);
}

async function openAboutWelcome() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    false
  );
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
  });
  return tab.linkedBrowser;
}

/**
 * Setup and test simplified welcome UI
 */
async function test_about_welcome(
  experiment,
  expectedSelectors = [],
  unexpectedSelectors = []
) {
  await setAboutWelcomePref(true);
  let browser = await openAboutWelcome();

  await ContentTask.spawn(
    browser,
    { expectedSelectors, experiment, unexpectedSelectors },
    async ({
      expectedSelectors: expected,
      experiment: experimentName,
      unexpectedSelectors: unexpected,
    }) => {
      for (let selector of expected) {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(selector),
          `Should render ${selector} in ${experimentName}`
        );
      }
      for (let selector of unexpected) {
        ok(
          !content.document.querySelector(selector),
          `Should not render ${selector} in ${experimentName}`
        );
      }
    }
  );
}

/**
 * Test the the various simplified welcome UI experiment values.
 * For now we have default content in place, UI content should be tested
 * for respective strings as we set content via experiment API
 */
add_task(async function test_Separate_About_Welcome_branches() {
  await test_about_welcome(
    "default",
    // Expected selectors:
    [
      "h1[data-l10n-id=onboarding-welcome-header]",
      "button[data-l10n-id=onboarding-data-sync-button2]",
      "button[data-l10n-id=onboarding-firefox-monitor-button]",
      "button[data-l10n-id=onboarding-browse-privately-button",
    ],
    // Unexpected selectors:
    [
      ".trailhead.welcomeCohort",
      ".welcome-subtitle",
      "h3[data-l10n-id=onboarding-welcome-form-header]",
      "p[data-l10n-id=onboarding-benefit-sync-text]",
      "p[data-l10n-id=onboarding-benefit-monitor-text]",
      "p[data-l10n-id=onboarding-benefit-lockwise-text]",
    ]
  );
});

/**
 * Test click of StartBrowsing button on simplified about:welcome
 * page changes focus to location bar
 */
test_newtab(
  {
    async before({ pushPrefs }) {
      await pushPrefs(["browser.aboutwelcome.enabled", true]);
    },
    test: async function test_startBrowsing() {
      await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector(
            "button[data-l10n-id=onboarding-start-browsing-button-label]"
          ),
        "Wait for start browsing button to load"
      );

      const startBrowsingButton = content.document.querySelector(
        "button[data-l10n-id=onboarding-start-browsing-button-label]"
      );

      startBrowsingButton.click();
    },
    after() {
      ok(
        gURLBar.focused,
        "Start Browsing click should move focus to awesome bar"
      );
    },
  },
  "about:welcome"
);
