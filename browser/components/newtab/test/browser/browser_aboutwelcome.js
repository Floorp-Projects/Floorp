"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const BRANCH_PREF = "trailhead.firstrun.branches";
const SIMPLIFIED_WELCOME_ENABLED_PREF = "browser.aboutwelcome.enabled";

/**
 * Sets the trailhead branch pref to the passed value.
 */
async function setTrailheadBranch(value) {
  Services.prefs.setCharPref(BRANCH_PREF, value);
  // Set about:welcome to use trailhead flow
  Services.prefs.setBoolPref(SIMPLIFIED_WELCOME_ENABLED_PREF, false);

  // Reset trailhead so it loads the new branch.
  Services.prefs.clearUserPref("trailhead.firstrun.didSeeAboutWelcome");
  await ASRouter.setState({ trailheadInitialized: false });

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(BRANCH_PREF);
    Services.prefs.clearUserPref(SIMPLIFIED_WELCOME_ENABLED_PREF);
  });
}

/**
 * Test a specific trailhead branch.
 */
async function test_trailhead_branch(
  branchName,
  expectedSelectors = [],
  unexpectedSelectors = []
) {
  await setTrailheadBranch(branchName);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    false
  );
  let browser = tab.linkedBrowser;

  await ContentTask.spawn(
    browser,
    { expectedSelectors, branchName, unexpectedSelectors },
    async ({
      expectedSelectors: expected,
      branchName: branch,
      unexpectedSelectors: unexpected,
    }) => {
      for (let selector of expected) {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(selector),
          `Should render ${selector} in the ${branch} branch`
        );
      }
      for (let selector of unexpected) {
        ok(
          !content.document.querySelector(selector),
          `Should not render ${selector} in the ${branch} branch`
        );
      }
    }
  );

  BrowserTestUtils.removeTab(tab);
}

/**
 * Test the the various trailhead branches.
 */
add_task(async function test_trailhead_branches() {
  await test_trailhead_branch(
    "join-dynamic",
    // Expected selectors:
    [
      ".trailhead.welcomeCohort",
      "button[data-l10n-id=onboarding-data-sync-button2]",
      "button[data-l10n-id=onboarding-firefox-monitor-button]",
      "button[data-l10n-id=onboarding-browse-privately-button]",
    ]
  );

  // Validate sync card is not shown if user usesFirefoxSync
  await pushPrefs(["services.sync.username", "someone@foo.com"]);
  await test_trailhead_branch(
    "join-dynamic",
    // Expected selectors:
    [
      ".trailhead.welcomeCohort",
      "button[data-l10n-id=onboarding-firefox-monitor-button]",
      "button[data-l10n-id=onboarding-browse-privately-button]",
    ],
    // Unexpected selectors:
    ["button[data-l10n-id=onboarding-data-sync-button2]"]
  );

  // Validate multidevice card is not shown if user has mobile devices connected
  await pushPrefs(["services.sync.clients.devices.mobile", 1]);
  await test_trailhead_branch(
    "join-dynamic",
    // Expected selectors:
    [
      ".trailhead.welcomeCohort",
      "button[data-l10n-id=onboarding-firefox-monitor-button]",
    ],
    // Unexpected selectors:
    ["button[data-l10n-id=onboarding-mobile-phone-button"]
  );

  await test_trailhead_branch(
    "sync-supercharge",
    // Expected selectors:
    [
      ".trailhead.syncCohort",
      "button[data-l10n-id=onboarding-data-sync-button2]",
      "button[data-l10n-id=onboarding-firefox-monitor-button]",
      "button[data-l10n-id=onboarding-mobile-phone-button]",
    ]
  );

  await test_trailhead_branch(
    "modal_variant_a-supercharge",
    // Expected selectors:
    [
      ".trailhead.welcomeCohort",
      "p[data-l10n-id=onboarding-benefit-sync-text]",
      "p[data-l10n-id=onboarding-benefit-monitor-text]",
      "p[data-l10n-id=onboarding-benefit-lockwise-text]",
    ]
  );

  await test_trailhead_branch(
    "modal_variant_f-supercharge",
    // Expected selectors:
    [
      ".trailhead.welcomeCohort",
      "h3[data-l10n-id=onboarding-welcome-form-header]",
      "p[data-l10n-id=onboarding-benefit-sync-text]",
      "p[data-l10n-id=onboarding-benefit-monitor-text]",
      "p[data-l10n-id=onboarding-benefit-lockwise-text]",
      "button[data-l10n-id=onboarding-join-form-signin]",
    ]
  );

  await test_trailhead_branch(
    "join-supercharge",
    // Expected selectors:
    [
      ".trailhead.welcomeCohort",
      "h3[data-l10n-id=onboarding-welcome-form-header]",
      "p[data-l10n-id=onboarding-benefit-sync-text]",
      "p[data-l10n-id=onboarding-benefit-monitor-text]",
      "p[data-l10n-id=onboarding-benefit-lockwise-text]",
      "button[data-l10n-id=onboarding-join-form-signin]",
    ]
  );

  await test_trailhead_branch(
    "full_page_d-supercharge",
    // Expected selectors:
    [
      ".trailhead-fullpage",
      ".trailheadCard",
      "p[data-l10n-id=onboarding-benefit-products-text]",
      "button[data-l10n-id=onboarding-join-form-continue]",
      "button[data-l10n-id=onboarding-join-form-signin]",
    ]
  );

  await test_trailhead_branch(
    "full_page_e-supercharge",
    // Expected selectors:
    [
      ".fullPageCardsAtTop",
      ".trailhead-fullpage",
      ".trailheadCard",
      "p[data-l10n-id=onboarding-benefit-products-text]",
      "button[data-l10n-id=onboarding-join-form-continue]",
      "button[data-l10n-id=onboarding-join-form-signin]",
    ]
  );

  await test_trailhead_branch(
    "nofirstrun",
    [],
    // Unexpected selectors:
    ["#trailheadDialog", ".trailheadCards"]
  );

  // Test trailhead default join-supercharge branch renders
  // correct when separate about welcome pref is false
  await test_trailhead_branch(
    "join-supercharge",
    // Expected selectors:
    [
      ".trailhead.welcomeCohort",
      "h1[data-l10n-id=onboarding-welcome-header]",
      "button[data-l10n-id=onboarding-firefox-monitor-button]",
      "h3[data-l10n-id=onboarding-welcome-form-header]",
      "p[data-l10n-id=onboarding-benefit-sync-text]",
      "p[data-l10n-id=onboarding-benefit-monitor-text]",
      "p[data-l10n-id=onboarding-benefit-lockwise-text]",
    ],
    ["h2[data-l10n-id=onboarding-fullpage-welcome-subheader]"]
  );
});
