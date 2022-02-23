"use strict";

const { ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const TEST_PROTON_CONTENT = [
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
        label: "link",
      },
      secondary_button_top: {
        label: "link top",
        action: {
          type: "SHOW_FIREFOX_ACCOUNTS",
          data: { entrypoint: "test" },
        },
      },
      help_text: {
        text: "Here's some sample help text",
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
      },
    },
  },
  {
    id: "AW_STEP3",
    order: 2,
    content: {
      title: "Step 3",
      tiles: {
        type: "theme",
        action: {
          theme: "<event>",
        },
        data: [
          {
            theme: "automatic",
            label: "theme-1",
            tooltip: "test-tooltip",
          },
          {
            theme: "dark",
            label: "theme-2",
          },
        ],
      },
      primary_button: {
        label: "Next",
        action: {
          navigate: true,
        },
      },
      secondary_button: {
        label: "Import",
        action: {
          type: "SHOW_MIGRATION_WIZARD",
          data: { source: "chrome" },
        },
      },
    },
  },
  {
    id: "AW_STEP4",
    order: 3,
    content: {
      title: "Step 4",
      primary_button: {
        label: "Next",
        action: {
          navigate: true,
        },
      },
      secondary_button: {
        label: "link",
      },
    },
  },
];

/**
 * Test the zero onboarding using ExperimentAPI
 */
add_task(async function test_multistage_zeroOnboarding_experimentAPI() {
  await setAboutWelcomePref(true);
  await ExperimentAPI.ready();
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    value: { enabled: false },
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );

  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
  });

  const browser = tab.linkedBrowser;

  await test_screen_content(
    browser,
    "Opens new tab",
    // Expected selectors:
    ["div.search-wrapper", "body.activity-stream"],
    // Unexpected selectors:
    ["div.onboardingContainer", "main.AW_STEP1"]
  );

  await doExperimentCleanup();
});

/**
 * Test the multistage welcome UI with test content theme as first screen
 */
add_task(async function test_multistage_aboutwelcome_experimentAPI() {
  const TEST_CONTENT = [
    {
      id: "AW_STEP1",
      order: 0,
      content: {
        title: "Step 1",
        tiles: {
          type: "theme",
          action: {
            theme: "<event>",
          },
          data: [
            {
              theme: "automatic",
              label: "theme-1",
              tooltip: "test-tooltip",
            },
            {
              theme: "dark",
              label: "theme-2",
            },
          ],
        },
        primary_button: {
          label: "Next",
          action: {
            navigate: true,
          },
        },
        secondary_button: {
          label: "link",
        },
        secondary_button_top: {
          label: "link top",
          action: {
            type: "SHOW_FIREFOX_ACCOUNTS",
            data: { entrypoint: "test" },
          },
        },
      },
    },
    {
      id: "AW_STEP2",
      order: 1,
      content: {
        zap: true,
        title: "Step 2 test",
        primary_button: {
          label: "Next",
          action: {
            navigate: true,
          },
        },
        secondary_button: {
          label: "link",
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
        secondary_button: {
          label: "Import",
          action: {
            type: "SHOW_MIGRATION_WIZARD",
            data: { source: "chrome" },
          },
        },
      },
    },
  ];
  const sandbox = sinon.createSandbox();
  NimbusFeatures.aboutwelcome._didSendExposureEvent = false;
  await setAboutWelcomePref(true);
  await ExperimentAPI.ready();

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    enabled: true,
    value: {
      id: "my-mochitest-experiment",
      screens: TEST_CONTENT,
    },
  });

  sandbox.spy(ExperimentAPI, "recordExposureEvent");

  Services.telemetry.clearScalars();
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );

  const browser = tab.linkedBrowser;

  let aboutWelcomeActor = await getAboutWelcomeParent(browser);
  // Stub AboutWelcomeParent Content Message Handler
  sandbox.spy(aboutWelcomeActor, "onContentMessage");
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
    sandbox.restore();
  });

  // Test first (theme) screen for non-win7.
  if (!win7Content) {
    await test_screen_content(
      browser,
      "multistage step 1",
      // Expected selectors:
      [
        "div.onboardingContainer",
        "main.AW_STEP1",
        "div.secondary-cta",
        "div.secondary-cta.top",
        "button[value='secondary_button']",
        "button[value='secondary_button_top']",
        "label.theme",
        "input[type='radio']",
      ],
      // Unexpected selectors:
      ["main.AW_STEP2", "main.AW_STEP3", "div.tiles-container.info"]
    );

    await onButtonClick(browser, "button.primary");

    const { callCount } = aboutWelcomeActor.onContentMessage;
    ok(callCount >= 1, `${callCount} Stub was called`);
    let clickCall;
    for (let i = 0; i < callCount; i++) {
      const call = aboutWelcomeActor.onContentMessage.getCall(i);
      info(`Call #${i}: ${call.args[0]} ${JSON.stringify(call.args[1])}`);
      if (call.calledWithMatch("", { event: "CLICK_BUTTON" })) {
        clickCall = call;
      }
    }

    Assert.equal(
      clickCall.args[0],
      "AWPage:TELEMETRY_EVENT",
      "send telemetry event"
    );

    Assert.equal(
      clickCall.args[1].message_id,
      "MY-MOCHITEST-EXPERIMENT_0_AW_STEP1",
      "Telemetry should join id defined in feature value with screen"
    );
  }

  await test_screen_content(
    browser,
    "multistage step 2",
    // Expected selectors:
    [
      "div.onboardingContainer",
      "main.AW_STEP2",
      "button[value='secondary_button']",
    ],
    // Unexpected selectors:
    ["main.AW_STEP1", "main.AW_STEP3", "div.secondary-cta.top"]
  );
  await onButtonClick(browser, "button.primary");
  await test_screen_content(
    browser,
    "multistage step 3",
    // Expected selectors:
    [
      "div.onboardingContainer",
      "main.AW_STEP3",
      "div.brand-logo",
      "div.welcome-text",
    ],
    // Unexpected selectors:
    ["main.AW_STEP1", "main.AW_STEP2"]
  );
  await onButtonClick(browser, "button.primary");
  await test_screen_content(
    browser,
    "home",
    // Expected selectors:
    ["body.activity-stream"],
    // Unexpected selectors:
    ["div.onboardingContainer"]
  );

  Assert.equal(
    ExperimentAPI.recordExposureEvent.callCount,
    1,
    "Called only once for exposure event"
  );

  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "telemetry.event_counts",
    "normandy#expose#nimbus_experiment",
    1
  );

  await doExperimentCleanup();
});

/**
 * Test the multistage proton welcome UI using ExperimentAPI with transitions
 */
add_task(async function test_multistage_aboutwelcome_transitions() {
  const sandbox = sinon.createSandbox();
  await setAboutWelcomePref(true);
  await ExperimentAPI.ready();

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    value: {
      id: "my-mochitest-experiment",
      enabled: true,
      screens: TEST_PROTON_CONTENT,
      transitions: true,
    },
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );

  const browser = tab.linkedBrowser;

  let aboutWelcomeActor = await getAboutWelcomeParent(browser);
  // Stub AboutWelcomeParent Content Message Handler
  sandbox.spy(aboutWelcomeActor, "onContentMessage");
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
    sandbox.restore();
  });

  await test_screen_content(
    browser,
    "multistage proton step 1",
    // Expected selectors:
    ["div.proton.transition- .screen-0"],
    // Unexpected selectors:
    ["div.proton.transition-out"]
  );

  // Double click should still only transition once.
  await onButtonClick(browser, "button.primary");
  await onButtonClick(browser, "button.primary");

  await test_screen_content(
    browser,
    "multistage proton step 1 transition to 2",
    // Expected selectors:
    ["div.proton.transition-out .screen-0", "div.proton.transition- .screen-1"]
  );

  await doExperimentCleanup();
});

/**
 * Test the multistage proton welcome UI using ExperimentAPI without transitions
 */
add_task(async function test_multistage_aboutwelcome_transitions_off() {
  const sandbox = sinon.createSandbox();
  await setAboutWelcomePref(true);
  await ExperimentAPI.ready();

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    value: {
      id: "my-mochitest-experiment",
      enabled: true,
      screens: TEST_PROTON_CONTENT,
      transitions: false,
    },
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );

  const browser = tab.linkedBrowser;

  let aboutWelcomeActor = await getAboutWelcomeParent(browser);
  // Stub AboutWelcomeParent Content Message Handler
  sandbox.spy(aboutWelcomeActor, "onContentMessage");
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
    sandbox.restore();
  });

  await test_screen_content(
    browser,
    "multistage proton step 1",
    // Expected selectors:
    ["div.proton.transition- .screen-0"],
    // Unexpected selectors:
    ["div.proton.transition-out"]
  );

  await onButtonClick(browser, "button.primary");
  await test_screen_content(
    browser,
    "multistage proton step 1 no transition to 2",
    // Expected selectors:
    [],
    // Unexpected selectors:
    ["div.proton.transition-out .screen-0"]
  );

  await doExperimentCleanup();
});
