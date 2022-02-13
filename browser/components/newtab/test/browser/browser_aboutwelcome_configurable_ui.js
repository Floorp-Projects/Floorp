"use strict";

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

const TEST_NOODLE_CONTENT = {
  id: "TEST_NOODLE_STEP",
  order: 0,
  content: Object.assign(
    {
      has_noodles: true,
    },
    BASE_SCREEN_CONTENT
  ),
};
const TEST_NOODLE_JSON = JSON.stringify([TEST_NOODLE_CONTENT]);

const TEST_LOGO_URL = "chrome://branding/content/icon64.png";
const TEST_LOGO_CONTENT = {
  id: "TEST_LOGO_STEP",
  order: 0,
  content: Object.assign(
    {
      logo: {
        size: "50px",
        imageURL: TEST_LOGO_URL,
      },
    },
    BASE_SCREEN_CONTENT
  ),
};
const TEST_LOGO_JSON = JSON.stringify([TEST_LOGO_CONTENT]);

async function openAboutWelcome(json) {
  await setAboutWelcomeMultiStage(json);

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
  let browser = await openAboutWelcome(TEST_NOODLE_JSON);

  await test_screen_content(
    browser,
    "renders screen with noodles",
    // Expected selectors:
    [
      "main.TEST_NOODLE_STEP",
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
  let browser = await openAboutWelcome(TEST_LOGO_JSON);
  const LOGO_SIZE = TEST_LOGO_CONTENT.content.logo.size;
  const EXPECTED_LOGO_STYLE = `background: rgba(0, 0, 0, 0) url("${TEST_LOGO_URL}") no-repeat scroll center top / ${LOGO_SIZE}; height: ${LOGO_SIZE}; padding: ${LOGO_SIZE} 0px 10px;`;
  const DEFAULT_LOGO_STYLE =
    'background: rgba(0, 0, 0, 0) url("chrome://branding/content/about-logo.svg") no-repeat scroll center top / 80px; height: 80px; padding: 80px 0px 10px;';

  await test_screen_content(
    browser,
    "renders screen with noodles",
    // Expected selectors:
    ["main.TEST_LOGO_STEP", `div.brand-logo[style*='${EXPECTED_LOGO_STYLE}']`],
    // Unexpected selectors:
    [`div.brand-logo[style*='${DEFAULT_LOGO_STYLE}`]
  );
});
