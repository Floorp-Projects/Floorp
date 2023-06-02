"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { AttributionCode } = ChromeUtils.importESModule(
  "resource:///modules/AttributionCode.sys.mjs"
);
const { AddonRepository } = ChromeUtils.importESModule(
  "resource://gre/modules/addons/AddonRepository.sys.mjs"
);

const TEST_ATTRIBUTION_DATA = {
  source: "addons.mozilla.org",
  medium: "referral",
  campaign: "non-fx-button",
  // with the sinon override, the id doesn't matter
  content: "rta:whatever",
};

const TEST_ADDON_INFO = [
  {
    name: "Test Add-on",
    sourceURI: { scheme: "https", spec: "https://test.xpi" },
    icons: { 32: "test.png", 64: "test.png" },
    type: "extension",
  },
];

const TEST_UA_ATTRIBUTION_DATA = {
  ua: "chrome",
};

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
      has_noodles: true,
    },
  },
  {
    id: "AW_STEP2",
    content: {
      title: "Step 2",
      primary_button: {
        label: {
          string_id: "onboarding-multistage-import-primary-button-label",
        },
        action: {
          type: "SHOW_MIGRATION_WIZARD",
          data: {},
        },
      },
      has_noodles: true,
    },
  },
];

async function openRTAMOWithAttribution() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(AddonRepository, "getAddonsByIDs").resolves(TEST_ADDON_INFO);

  await AttributionCode.deleteFileAsync();
  await ASRouter.forceAttribution(TEST_ATTRIBUTION_DATA);

  AttributionCode._clearCache();
  const data = await AttributionCode.getAttrDataAsync();

  Assert.equal(
    data.source,
    "addons.mozilla.org",
    "Attribution data should be set"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );
  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
    await ASRouter.forceAttribution("");
    sandbox.restore();
  });
  return tab.linkedBrowser;
}

/**
 * Setup and test RTAMO welcome UI
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
    }
  );
}

add_task(async function test_rtamo_attribution() {
  let browser = await openRTAMOWithAttribution();

  await test_screen_content(
    browser,
    "RTAMO UI",
    // Expected selectors:
    [
      "div.onboardingContainer",
      "h2[data-l10n-id='mr1-return-to-amo-addon-title']",
      "div.rtamo-icon",
      "button.primary",
      "button.secondary",
    ],
    // Unexpected selectors:
    [
      "main.AW_STEP1",
      "main.AW_STEP2",
      "main.AW_STEP3",
      "div.tiles-container.info",
    ]
  );
});

async function openMultiStageWithUserAgentAttribution() {
  const sandbox = sinon.createSandbox();
  await ASRouter.forceAttribution(TEST_UA_ATTRIBUTION_DATA);
  const TEST_PROTON_JSON = JSON.stringify(TEST_PROTON_CONTENT);

  await setAboutWelcomePref(true);
  await pushPrefs(["browser.aboutwelcome.screens", TEST_PROTON_JSON]);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );
  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
    await ASRouter.forceAttribution("");
    sandbox.restore();
  });
  return tab.linkedBrowser;
}

async function onButtonClick(browser, elementId) {
  await ContentTask.spawn(
    browser,
    { elementId },
    async ({ elementId: buttonId }) => {
      await ContentTaskUtils.waitForCondition(
        () => content.document.querySelector(buttonId),
        buttonId
      );
      let button = content.document.querySelector(buttonId);
      button.click();
    }
  );
}

add_task(async function test_ua_attribution() {
  let browser = await openMultiStageWithUserAgentAttribution();

  await test_screen_content(
    browser,
    "multistage step 1 with ua attribution",
    // Expected selectors:
    ["div.onboardingContainer", "main.AW_STEP1", "button.primary"],
    // Unexpected selectors:
    ["main.AW_STEP2"]
  );

  await onButtonClick(browser, "button.primary");

  await test_screen_content(
    browser,
    "multistage step 2 with ua attribution",
    // Expected selectors:
    [
      "div.onboardingContainer",
      "main.AW_STEP2",
      "button.primary[data-l10n-args*='Google Chrome']",
    ],
    // Unexpected selectors:
    ["main.AW_STEP1"]
  );
});
