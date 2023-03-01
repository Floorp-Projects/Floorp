"use strict";

const { AboutWelcomeParent } = ChromeUtils.import(
  "resource:///actors/AboutWelcomeParent.jsm"
);

const TEST_PROTON_CONTENT = [
  {
    id: "AW_PIN_FIREFOX_STEP1",
    content: {
      position: "corner",
      title: "Step 1",
      primary_button: {
        label: "Pin Firefox",
        action: {
          navigate: true,
        },
      },
      has_noodles: true,
    },
  },
  {
    id: "AW_SET_DEFAULT_STEP2",
    content: {
      title: "Step 2",
      primary_button: {
        label: "Set Default",
        action: {
          navigate: true,
        },
      },
      secondary_button: {
        label: "Next",
        action: {
          navigate: true,
        },
      },
      has_noodles: true,
    },
  },
];

const TEST_PROTON_JSON = JSON.stringify(TEST_PROTON_CONTENT);

async function openAboutWelcome() {
  await setAboutWelcomePref(true);
  await setAboutWelcomeMultiStage(TEST_PROTON_JSON);

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

add_task(async function test_AWMultistage_RemovePinScreen() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);

  const sandbox = sinon.createSandbox();
  // Simulate Firefox as not pinned by stubbing doesAppNeedPin
  const pinStub = sandbox
    .stub(AboutWelcomeParent, "doesAppNeedPin")
    .returns(true);

  // Simulate Firefox as not default by stubbing isDefaultBrowser
  sandbox.stub(AboutWelcomeParent, "isDefaultBrowser").returns(false);

  let browser = await openAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);

  // Spy AboutWelcomeParent Content Message Handler
  sandbox.spy(aboutWelcomeActor, "onContentMessage");

  registerCleanupFunction(() => {
    sandbox.restore();
  });

  // Click primary button on first screen
  await onButtonClick(browser, "button.primary");

  // Simulate Firefox app as pinned by stubbing doesAppNeedPin as false
  pinStub.returns(false);

  // Test second screen content
  await test_screen_content(
    browser,
    "multistage step 2",
    // Expected selectors:
    ["main.AW_SET_DEFAULT_STEP2"],
    // Unexpected selectors:
    ["main.AW_PIN_FIREFOX_STEP1"]
  );

  await onButtonClick(browser, "button.secondary");
  const { callCount } = aboutWelcomeActor.onContentMessage;
  ok(
    callCount >= 2,
    `${callCount} Stub called twice to handle click action and Telemetry`
  );

  for (let i = 0; i < callCount; i++) {
    const call = aboutWelcomeActor.onContentMessage.getCall(i);
    info(`Call #${i}: ${call.args[0]} ${JSON.stringify(call.args[1])}`);
  }

  // Test about:home screen content
  await test_screen_content(
    browser,
    "home",
    // Expected selectors:
    ["body.activity-stream"],
    // Unexpected selectors:
    ["div.onboardingContainer"]
  );

  // Go back to about:welcome
  await TestUtils.waitForCondition(() => browser.canGoBack);
  browser.goBack();

  // Onboarding flow should have pin firefox screen removed
  // and have only one screen with screen id AW_ONLY_DEFAULT
  await test_screen_content(
    browser,
    "multistage proton step 1",
    // Expected selectors:
    ["main.AW_ONLY_DEFAULT"],
    // Unexpected selectors:
    ["main.AW_PIN_FIREFOX_STEP1"]
  );

  // Ensure step indicator is not displayed
  await test_element_styles(
    browser,
    "div.steps",
    // Expected styles:
    {
      display: "none",
    }
  );
});
