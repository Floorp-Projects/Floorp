"use strict";

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
  await setProton(false);
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
      "div.onboardingContainer",
      "nav.steps",
      "button.primary",
      "button.secondary",
    ],
    // Unexpected selectors:
    [".trailhead.welcomeCohort", ".welcome-subtitle"]
  );
});
