"use strict";

const TEST_DEFAULT_CONTENT = [
  {
    id: "AW_STEP1",
    content: {
      title: "Step 1",
      primary_button: {
        label: "Next",
        action: {
          navigate: true,
        },
      },
      secondary_button: {
        label: "Secondary",
      },
    },
  },
  {
    id: "AW_STEP2",
    targeting: "false",
    content: {
      title: "Step 2",
      primary_button: {
        label: "Next",
        action: {
          navigate: true,
        },
      },
      secondary_button: {
        label: "Secondary",
      },
    },
  },
  {
    id: "AW_STEP3",
    content: {
      title: "Step 3",
      primary_button: {
        label: "Next",
        action: {
          navigate: true,
        },
      },
      secondary_button: {
        label: "Secondary",
      },
    },
  },
];

const sandbox = sinon.createSandbox();

add_setup(function initSandbox() {
  requestLongerTimeout(2);

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

const TEST_DEFAULT_JSON = JSON.stringify(TEST_DEFAULT_CONTENT);
async function openAboutWelcome() {
  await setAboutWelcomePref(true);
  await setAboutWelcomeMultiStage(TEST_DEFAULT_JSON);

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

add_task(async function second_screen_filtered_by_targeting() {
  let browser = await openAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);
  // Stub AboutWelcomeParent Content Message Handler
  sandbox.spy(aboutWelcomeActor, "onContentMessage");

  await test_screen_content(
    browser,
    "multistage step 1",
    // Expected selectors:
    ["main.AW_STEP1"],
    // Unexpected selectors:
    ["main.AW_STEP2", "main.AW_STEP3"]
  );

  await onButtonClick(browser, "button.primary");

  await test_screen_content(
    browser,
    "multistage step 3",
    // Expected selectors:
    ["main.AW_STEP3"],
    // Unexpected selectors:
    ["main.AW_STEP2", "main.AW_STEP1"]
  );

  sandbox.restore();
  await popPrefs();
});

/**
 * Test MR template easy setup content - Browser is pinned and
 * not set as default and Windows 10 version 1703
 */
add_task(async function test_aboutwelcome_mr_template_easy_setup() {
  await pushPrefs([
    "browser.migrate.content-modal.about-welcome-behavior",
    "default",
  ]);

  if (!AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
    return;
  }

  if (
    //Windows version 1703
    TelemetryEnvironment.currentEnvironment.system.os.windowsBuildNumber < 15063
  ) {
    return;
  }

  sandbox.stub(ShellService, "doesAppNeedPin").returns(false);
  sandbox.stub(ShellService, "isDefaultBrowser").returns(false);

  await clearHistoryAndBookmarks();

  const { browser, cleanup } = await openMRAboutWelcome();

  //should render easy setup
  await test_screen_content(
    browser,
    "doesn't render pin, import and set to default",
    //Expected selectors:
    ["main.AW_EASY_SETUP"],
    //Unexpected selectors:
    ["main.AW_PIN_FIREFOX", "main.AW_SET_DEFAULT", "main.AW_IMPORT_SETTINGS"]
  );

  await cleanup();
  await popPrefs();
  sandbox.restore();
});
