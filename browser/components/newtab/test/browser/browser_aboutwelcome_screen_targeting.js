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
 * Test MR template easy setup default content - Browser is not pinned
 * and not set as default
 */
add_task(async function test_aboutwelcome_mr_template_easy_setup_default() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);
  sandbox.stub(ShellService, "doesAppNeedPin").returns(true);
  sandbox.stub(ShellService, "isDefaultBrowser").returns(false);

  await clearHistoryAndBookmarks();

  const { browser, cleanup } = await openMRAboutWelcome();

  //should render easy setup default experience
  await test_screen_content(
    browser,
    "doesn't render only pin, default, or import easy setup",
    //Expected selectors:
    ["main.AW_EASY_SETUP_NEEDS_DEFAULT_AND_PIN"],
    //Unexpected selectors:
    [
      "main.AW_EASY_SETUP_NEEDS_DEFAULT",
      "main.AW_EASY_SETUP_NEEDS_PIN",
      "main.AW_ONLY_IMPORT",
    ]
  );

  await cleanup();
  await popPrefs();
  sandbox.restore();
});

/**
 * Test MR template easy setup content - Browser is not pinned
 * and set as default
 */
add_task(async function test_aboutwelcome_mr_template_easy_setup_needs_pin() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);
  sandbox.stub(ShellService, "doesAppNeedPin").returns(true);
  sandbox.stub(ShellService, "isDefaultBrowser").returns(true);

  await clearHistoryAndBookmarks();

  const { browser, cleanup } = await openMRAboutWelcome();

  //should render easy setup needs pin
  await test_screen_content(
    browser,
    "doesn't render default and pin, only default or import easy setup",
    //Expected selectors:
    ["main.AW_EASY_SETUP_NEEDS_PIN"],
    //Unexpected selectors:
    [
      "main.AW_EASY_SETUP_NEEDS_DEFAULT",
      "main.AW_EASY_SETUP_NEEDS_DEFAULT_AND_PIN",
      "main.AW_ONLY_IMPORT",
    ]
  );

  await cleanup();
  await popPrefs();
  sandbox.restore();
});

/**
 * Test MR template easy setup content - Browser is pinned and
 * not set as default
 */
add_task(
  async function test_aboutwelcome_mr_template_easy_setup_needs_default() {
    await pushPrefs(["browser.shell.checkDefaultBrowser", true]);
    sandbox.stub(ShellService, "doesAppNeedPin").returns(false);
    sandbox.stub(ShellService, "isDefaultBrowser").returns(false);

    await clearHistoryAndBookmarks();

    const { browser, cleanup } = await openMRAboutWelcome();

    //should render easy setup needs default
    await test_screen_content(
      browser,
      "doesn't render pin, import and set to default",
      //Expected selectors:
      ["main.AW_EASY_SETUP_NEEDS_DEFAULT"],
      //Unexpected selectors:
      [
        "main.AW_EASY_SETUP_NEEDS_PIN",
        "main.AW_EASY_SETUP_NEEDS_DEFAULT_AND_PIN",
        "main.AW_ONLY_IMPORT",
      ]
    );

    await cleanup();
    await popPrefs();
    sandbox.restore();
  }
);

/**
 * Test MR template easy setup content - Browser is pinned and
 * set as default
 */
add_task(async function test_aboutwelcome_mr_template_easy_setup_only_import() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);
  sandbox.stub(ShellService, "doesAppNeedPin").returns(false);
  sandbox.stub(ShellService, "isDefaultBrowser").returns(true);

  await clearHistoryAndBookmarks();

  const { browser, cleanup } = await openMRAboutWelcome();

  //should render easy setup - only import
  await test_screen_content(
    browser,
    "doesn't render any combination of pin and default",
    //Expected selectors:
    ["main.AW_EASY_SETUP_ONLY_IMPORT"],
    //Unexpected selectors:
    [
      "main.AW_EASY_SETUP_NEEDS_PIN",
      "main.AW_EASY_SETUP_NEEDS_DEFAULT_AND_PIN",
      "main.AW_EASY_SETUP_NEEDS_DEFAULT",
    ]
  );

  await cleanup();
  await popPrefs();
  sandbox.restore();
});
