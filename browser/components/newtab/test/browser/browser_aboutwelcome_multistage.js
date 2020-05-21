"use strict";

const SEPARATE_ABOUT_WELCOME_PREF = "browser.aboutwelcome.enabled";
const ABOUT_WELCOME_OVERRIDE_CONTENT_PREF =
  "browser.aboutwelcome.overrideContent";

const TEST_MULTISTAGE_JSON = JSON.stringify({
  id: "multi-stage-welcome",
  screens: [
    {
      id: "AW_STEP1",
      order: 0,
      content: {
        title: "Step 1",
        primary_button: {
          label: "Next",
          action: {
            navigate: true,
          },
        },
        secondary_button: {
          label: "link top",
          position: "top",
        },
      },
    },
    {
      id: "AW_STEP2",
      order: 1,
      content: {
        title: "Step 2",
        primary_button: {
          label: "Next",
          action: {
            navigate: true,
          },
        },
        secondary_button: {
          label: "link",
          position: "bottom",
        },
      },
    },
    {
      id: "AW_STEP3",
      order: 2,
      content: {
        title: "Step 3",
        primary_button: {
          label: "Next",
          action: {
            navigate: true,
          },
        },
      },
    },
  ],
});

/**
 * Sets the aboutwelcome pref to enabled simplified welcome UI
 */
async function setAboutWelcomePref(value) {
  return pushPrefs([SEPARATE_ABOUT_WELCOME_PREF, value]);
}

async function setAboutWelcomeMultiStage(value) {
  return pushPrefs([ABOUT_WELCOME_OVERRIDE_CONTENT_PREF, value]);
}

async function openAboutWelcome() {
  await setAboutWelcomePref(true);
  await setAboutWelcomeMultiStage(TEST_MULTISTAGE_JSON);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
  });
  return tab.linkedBrowser;
}

/**
 * Setup and test simplified welcome UI
 */
async function test_screen_content(
  browser,
  experiment,
  expectedSelectors = [],
  unexpectedSelectors = []
) {
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

      if (experimentName === "home") {
        Assert.equal(
          content.document.location.href,
          "about:home",
          "Navigated to about:home"
        );
      } else {
        Assert.equal(
          content.document.location.href,
          "about:welcome",
          "Navigated to a welcome screen"
        );
      }
    }
  );
}

async function onNavigate(browser, message) {
  await ContentTask.spawn(
    browser,
    { message },
    async ({ message: messageText }) => {
      await ContentTaskUtils.waitForCondition(
        () => content.document.querySelector("button.primary"),
        messageText
      );
      let button = content.document.querySelector("button.primary");
      button.click();
    }
  );
}

/**
 * Test the multistage welcome UI rendered using TEST_MULTISTAGE_JSON
 */
add_task(async function test_Multistage_About_Welcome_branches() {
  let browser = await openAboutWelcome();

  await test_screen_content(
    browser,
    "multistage step 1",
    // Expected selectors:
    [
      "div.multistageContainer",
      "main.AW_STEP1",
      "div.secondary-cta.top",
      "button.secondary",
      "div.indicator.current",
    ],
    // Unexpected selectors:
    ["main.AW_STEP2", "main.AW_STEP3"]
  );

  await onNavigate(browser, "Step 1");
  await test_screen_content(
    browser,
    "multistage step 2",
    // Expected selectors:
    ["div.multistageContainer", "main.AW_STEP2", "button.secondary"],
    // Unexpected selectors:
    ["main.AW_STEP1", "main.AW_STEP3", "div.secondary-cta.top"]
  );
  await onNavigate(browser, "Step 2");
  await test_screen_content(
    browser,
    "multistage step 3",
    // Expected selectors:
    [
      "div.multistageContainer",
      "main.AW_STEP3",
      "div.brand-logo",
      "div.welcome-text",
    ],
    // Unexpected selectors:
    ["main.AW_STEP1", "main.AW_STEP2"]
  );
  await onNavigate(browser, "Step 3");
  await test_screen_content(
    browser,
    "home",
    // Expected selectors:
    ["body.activity-stream"],
    // Unexpected selectors:
    ["div.multistageContainer"]
  );
});
