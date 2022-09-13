"use strict";

const MR_TEMPLATE_PREF = "browser.aboutwelcome.templateMR";

const { ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { AboutWelcomeParent } = ChromeUtils.import(
  "resource:///actors/AboutWelcomeParent.jsm"
);
const { OnboardingMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/OnboardingMessageProvider.jsm"
);

async function openMRAboutWelcome() {
  await pushPrefs([MR_TEMPLATE_PREF, true]);
  await setAboutWelcomePref(true); // NB: Calls pushPrefs
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );

  return {
    browser: tab.linkedBrowser,
    cleanup: async () => {
      BrowserTestUtils.removeTab(tab);
      await popPrefs(); // for setAboutWelcomePref()
      await popPrefs(); // for pushPrefs()
    },
  };
}

async function clickVisibleButton(browser, selector) {
  // eslint-disable-next-line no-shadow
  await ContentTask.spawn(browser, { selector }, async ({ selector }) => {
    function getVisibleElement() {
      for (const el of content.document.querySelectorAll(selector)) {
        if (el.offsetParent !== null) {
          return el;
        }
      }
      return null;
    }
    await ContentTaskUtils.waitForCondition(
      getVisibleElement,
      selector,
      200, // interval
      100 // maxTries
    );
    getVisibleElement().click();
  });
}

function initSandbox({ pin = true, isDefault = false } = {}) {
  const sandbox = sinon.createSandbox();
  sandbox.stub(AboutWelcomeParent, "doesAppNeedPin").returns(pin);
  sandbox.stub(AboutWelcomeParent, "isDefaultBrowser").returns(isDefault);

  return sandbox;
}

/**
 * Test MR message telemetry
 */
add_task(async function test_aboutwelcome_mr_template_telemetry() {
  let { browser, cleanup } = await openMRAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);

  // Stub AboutWelcomeParent's Content Message Handler
  const sandbox = initSandbox();
  const messageStub = sandbox.spy(aboutWelcomeActor, "onContentMessage");

  await clickVisibleButton(browser, "button.secondary");

  const { callCount } = messageStub;
  ok(callCount >= 1, `${callCount} Stub was called`);
  let clickCall;
  for (let i = 0; i < callCount; i++) {
    const call = messageStub.getCall(i);
    info(`Call #${i}: ${call.args[0]} ${JSON.stringify(call.args[1])}`);
    if (call.calledWithMatch("", { event: "CLICK_BUTTON" })) {
      clickCall = call;
    }
  }

  Assert.ok(
    clickCall.args[1].message_id.startsWith("MR_WELCOME_DEFAULT"),
    "Telemetry includes MR message id"
  );

  await cleanup();
  sandbox.restore();
});

/**
 * Test MR template content - Browser is not Pinned and not set as default
 */
add_task(async function test_aboutwelcome_mr_template_content() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);

  const sandbox = initSandbox();

  let { browser, cleanup } = await openMRAboutWelcome();

  await test_screen_content(
    browser,
    "MR template includes screens with split position and a sign in link on the first screen",
    // Expected selectors:
    [
      `main.screen[pos="split"]`,
      "div.secondary-cta.top",
      "button[value='secondary_button_top']",
    ]
  );

  await test_screen_content(
    browser,
    "renders pin screen",
    //Expected selectors:
    ["main.AW_PIN_FIREFOX"],
    //Unexpected selectors:
    ["main.AW_GRATITUDE"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");

  //should render set default
  await test_screen_content(
    browser,
    "renders set default screen",
    //Expected selectors:
    ["main.AW_SET_DEFAULT"],
    //Unexpected selectors:
    ["main.AW_CHOOSE_THEME"]
  );

  await cleanup();
  sandbox.restore();
  await popPrefs();
});

/**
 * Test MR template content - Browser has been set as Default, not pinned
 */
add_task(async function test_aboutwelcome_mr_template_content_pin() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);

  const sandbox = initSandbox({ isDefault: true });

  let { browser, cleanup } = await openMRAboutWelcome();

  await test_screen_content(
    browser,
    "renders pin screen",
    //Expected selectors:
    ["main.AW_PIN_FIREFOX"],
    //Unexpected selectors:
    ["main.AW_SET_DEFAULT"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");

  await test_screen_content(
    browser,
    "renders next screen",
    //Expected selectors:
    ["main"],
    //Unexpected selectors:
    ["main.AW_SET_DEFAULT"]
  );

  await cleanup();
  sandbox.restore();
  await popPrefs();
});

/**
 * Test MR template content - Browser is Pinned, not default
 */
add_task(async function test_aboutwelcome_mr_template_only_default() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);

  const sandbox = initSandbox({ pin: false });

  let { browser, cleanup } = await openMRAboutWelcome();

  //should render set default
  await test_screen_content(
    browser,
    "renders set default screen",
    //Expected selectors:
    ["main.AW_ONLY_DEFAULT"],
    //Unexpected selectors:
    ["main.AW_PIN_FIREFOX"]
  );

  await cleanup();
  sandbox.restore();
  await popPrefs();
});
/**
 * Test MR template content - Browser is Pinned and set as default
 */
add_task(async function test_aboutwelcome_mr_template_get_started() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);

  const sandbox = initSandbox({ pin: false, isDefault: true });

  let { browser, cleanup } = await openMRAboutWelcome();

  //should render set default
  await test_screen_content(
    browser,
    "doesn't render pin and set default screens",
    //Expected selectors:
    ["main.AW_GET_STARTED"],
    //Unexpected selectors:
    [
      "main.AW_PIN_FIREFOX",
      "main.AW_ONLY_DEFAULT",
      ".action-buttons .secondary",
    ]
  );

  await cleanup();
  sandbox.restore();
  await popPrefs();
});

add_task(async function test_aboutwelcome_show_firefox_view() {
  const TEST_CONTENT = [
    {
      id: "AW_GRATITUDE",
      content: {
        position: "split",
        split_narrow_bkg_position: "-228px",
        background:
          "url('chrome://activity-stream/content/data/content/assets/mr-gratitude.svg') var(--mr-secondary-position) no-repeat, var(--mr-screen-background-color)",
        progress_bar: true,
        logo: {},
        title: {
          string_id: "mr2022-onboarding-gratitude-title",
        },
        subtitle: {
          string_id: "mr2022-onboarding-gratitude-subtitle",
        },
        primary_button: {
          label: {
            string_id: "mr2022-onboarding-gratitude-primary-button-label",
          },
          action: {
            type: "OPEN_FIREFOX_VIEW",
            navigate: true,
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2022-onboarding-gratitude-secondary-button-label",
          },
          action: {
            navigate: true,
          },
        },
      },
    },
  ];
  await setAboutWelcomeMultiStage(JSON.stringify(TEST_CONTENT)); // NB: calls SpecialPowers.pushPrefEnv
  let { cleanup, browser } = await openMRAboutWelcome();

  // execution
  await test_screen_content(
    browser,
    //Expected selectors
    ["main.UPGRADE_GRATITUDE"],
    //Unexpected selectors:
    []
  );
  await clickVisibleButton(browser, ".action-buttons button.primary");

  // verification
  await BrowserTestUtils.waitForEvent(gBrowser, "TabSwitchDone");
  assertFirefoxViewTabSelected(gBrowser.ownerGlobal);

  // cleanup
  await SpecialPowers.popPrefEnv(); // for setAboutWelcomeMultiStage
  closeFirefoxViewTab();
  await cleanup();
});

add_task(async function test_mr2022_templateMR() {
  const message = await OnboardingMessageProvider.getMessages().then(msgs =>
    msgs.find(m => m.id === "FX_MR_106_UPGRADE")
  );

  const screensJSON = JSON.stringify(message.content.screens);

  await setAboutWelcomeMultiStage(screensJSON); // NB: calls SpecialPowers.pushPrefEnv

  async function runMajorReleaseTest(
    {
      onboarding = undefined,
      templateMR = undefined,
      fallbackPref = undefined,
    },
    expected
  ) {
    info("Testing aboutwelcome layout with:");
    info(`  majorRelease2022.onboarding=${onboarding}`);
    info(`  aboutwelcome.templateMR=${templateMR}`);
    info(`  ${MR_TEMPLATE_PREF}=${fallbackPref}`);

    let mr2022Cleanup = async () => {};
    let aboutWelcomeCleanup = async () => {};

    if (typeof onboarding !== "undefined") {
      mr2022Cleanup = await ExperimentFakes.enrollWithFeatureConfig({
        featureId: "majorRelease2022",
        value: { onboarding },
      });
    }

    if (typeof templateMR !== "undefined") {
      aboutWelcomeCleanup = await ExperimentFakes.enrollWithFeatureConfig({
        featureId: "aboutwelcome",
        value: { templateMR },
      });
    }

    if (typeof fallbackPref !== "undefined") {
      await SpecialPowers.pushPrefEnv({
        set: [[MR_TEMPLATE_PREF, fallbackPref]],
      });
    }

    const tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:welcome",
      true
    );

    const SELECTOR = `main.screen[pos="split"]`;
    const args = expected
      ? ["split layout", [SELECTOR], []]
      : ["non-split layout", [], [SELECTOR]];

    try {
      await test_screen_content(tab.linkedBrowser, ...args);
    } finally {
      BrowserTestUtils.removeTab(tab);

      if (typeof fallbackPref !== "undefined") {
        await SpecialPowers.popPrefEnv();
      }

      await mr2022Cleanup();
      await aboutWelcomeCleanup();
    }
  }

  await ExperimentAPI.ready();

  await runMajorReleaseTest({ onboarding: true }, true);
  await runMajorReleaseTest({ onboarding: true, templateMR: false }, true);
  await runMajorReleaseTest({ onboarding: true, fallbackPref: false }, true);

  await runMajorReleaseTest({ onboarding: false }, false);
  await runMajorReleaseTest({ onboarding: false, templateMR: true }, false);
  await runMajorReleaseTest({ onboarding: false, fallbackPref: true }, false);

  await runMajorReleaseTest({ templateMR: true }, true);
  await runMajorReleaseTest({ templateMR: true, fallbackPref: false }, true);
  await runMajorReleaseTest({ fallbackPref: true }, true);

  await runMajorReleaseTest({ templateMR: false }, false);
  await runMajorReleaseTest({ templateMR: false, fallbackPref: true }, false);
  await runMajorReleaseTest({ fallbackPref: false }, false);

  // Check the default case with no experiments or prefs set.
  await runMajorReleaseTest({}, true);

  await SpecialPowers.popPrefEnv(); // for setAboutWelcomeMultiStage(screensJSON)
});
