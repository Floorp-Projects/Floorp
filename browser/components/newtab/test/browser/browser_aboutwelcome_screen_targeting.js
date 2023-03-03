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
  const sandbox = sinon.createSandbox();
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
});
