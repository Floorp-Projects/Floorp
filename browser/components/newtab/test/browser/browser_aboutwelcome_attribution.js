"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { AttributionCode } = ChromeUtils.import(
  "resource:///modules/AttributionCode.jsm"
);
const { AddonRepository } = ChromeUtils.import(
  "resource://gre/modules/addons/AddonRepository.jsm"
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
      "div.brand-logo",
      "h2[data-l10n-id='return-to-amo-addon-title']",
      "img[data-l10n-name='icon']",
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
