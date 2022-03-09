"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);

const BASE_SCREEN_CONTENT = {
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
};

const makeTestContent = (id, contentAdditions) => {
  return {
    id,
    order: 0,
    content: Object.assign({}, BASE_SCREEN_CONTENT, contentAdditions),
  };
};

async function openAboutWelcome(json) {
  if (json) {
    await setAboutWelcomeMultiStage(json);
  }

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
 * Test rendering a screen in about welcome with decorative noodles
 */
add_task(async function test_aboutwelcome_with_noodles() {
  const TEST_NOODLE_CONTENT = makeTestContent("TEST_NOODLE_STEP", {
    has_noodles: true,
  });
  const TEST_NOODLE_JSON = JSON.stringify([TEST_NOODLE_CONTENT]);
  let browser = await openAboutWelcome(TEST_NOODLE_JSON);

  await test_screen_content(
    browser,
    "renders screen with noodles",
    // Expected selectors:
    [
      "main.TEST_NOODLE_STEP[pos='center']",
      "div.noodle.purple-C",
      "div.noodle.orange-L",
      "div.noodle.outline-L",
      "div.noodle.yellow-circle",
    ]
  );
});

/**
 * Test rendering a screen with a customized logo
 */
add_task(async function test_aboutwelcome_with_customized_logo() {
  const TEST_LOGO_URL = "chrome://branding/content/icon64.png";
  const TEST_LOGO_CONTENT = makeTestContent("TEST_LOGO_STEP", {
    logo: {
      height: "50px",
      imageURL: TEST_LOGO_URL,
    },
  });
  const TEST_LOGO_JSON = JSON.stringify([TEST_LOGO_CONTENT]);
  let browser = await openAboutWelcome(TEST_LOGO_JSON);
  const LOGO_HEIGHT = TEST_LOGO_CONTENT.content.logo.height;
  const EXPECTED_LOGO_STYLE = `background: rgba(0, 0, 0, 0) url("${TEST_LOGO_URL}") no-repeat scroll center center / contain; height: ${LOGO_HEIGHT}`;
  const DEFAULT_LOGO_STYLE =
    'background: rgba(0, 0, 0, 0) url("chrome://branding/content/about-logo.svg")';

  await test_screen_content(
    browser,
    "renders screen with customized logo",
    // Expected selectors:
    [
      "main.TEST_LOGO_STEP[pos='center']",
      `div.brand-logo[style*='${EXPECTED_LOGO_STYLE}']`,
    ],
    // Unexpected selectors:
    [`div.brand-logo[style*='${DEFAULT_LOGO_STYLE}']`]
  );
});

/**
 * Test rendering a screen with a URL value and default color for backdrop
 */
add_task(async function test_aboutwelcome_with_url_backdrop() {
  const TEST_BACKDROP_URL = `url("chrome://activity-stream/content/data/content/assets/proton-bkg.avif")`;
  const TEST_BACKDROP_VALUE = `#212121 ${TEST_BACKDROP_URL} center/cover no-repeat fixed`;
  const TEST_URL_BACKDROP_CONTENT = makeTestContent("TEST_URL_BACKDROP_STEP");

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    enabled: true,
    value: {
      backdrop: TEST_BACKDROP_VALUE,
      screens: [TEST_URL_BACKDROP_CONTENT],
    },
  });
  let browser = await openAboutWelcome();

  await test_screen_content(
    browser,
    "renders screen with background image",
    // Expected selectors:
    [`div.outer-wrapper.onboardingContainer[style*='${TEST_BACKDROP_URL}']`]
  );
  await doExperimentCleanup();
});

/**
 * Test rendering a screen with a color name for backdrop
 */
add_task(async function test_aboutwelcome_with_color_backdrop() {
  const TEST_BACKDROP_COLOR = "transparent";
  const TEST_BACKDROP_COLOR_CONTENT = makeTestContent(
    "TEST_COLOR_NAME_BACKDROP_STEP"
  );

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    enabled: true,
    value: {
      backdrop: TEST_BACKDROP_COLOR,
      screens: [TEST_BACKDROP_COLOR_CONTENT],
    },
  });
  let browser = await openAboutWelcome();

  await test_screen_content(
    browser,
    "renders screen with background color",
    // Expected selectors:
    [`div.outer-wrapper.onboardingContainer[style*='${TEST_BACKDROP_COLOR}']`]
  );
  await doExperimentCleanup();
});
