"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const TEST_PROTON_CONTENT = [
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
        label: "link",
      },
      secondary_button_top: {
        label: "link top",
        action: {
          type: "SHOW_FIREFOX_ACCOUNTS",
          data: { entrypoint: "test" },
        },
      },
      has_noodles: true,
    },
  },
  {
    id: "AW_STEP2",
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
      has_noodles: true,
    },
  },
  {
    id: "AW_STEP3",
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
      has_noodles: true,
    },
  },
  {
    id: "AW_STEP4",
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
      has_noodles: true,
    },
  },
];

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
    ["div.proton.transition- .screen"],
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
    ["div.proton.transition-out .screen", "div.proton.transition- .screen-1"]
  );

  doExperimentCleanup();
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
    ["div.proton.transition- .screen"],
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

  doExperimentCleanup();
});
