"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { AddonRepository } = ChromeUtils.import(
  "resource://gre/modules/addons/AddonRepository.jsm"
);
const { AttributionCode } = ChromeUtils.import(
  "resource:///modules/AttributionCode.jsm"
);

async function openRTAMOWelcomePage() {
  let sandbox = sinon.createSandbox();
  // Can't properly stub the child/parent actors so instead
  // we stub the modules they depend on for the RTAMO flow
  // to ensure the right thing is rendered.
  await ASRouter.forceAttribution({
    source: "addons.mozilla.org",
    medium: "referral",
    campaign: "non-fx-button",
    // with the sinon override, the id doesn't matter
    content: "rta:whatever",
    experiment: "ua-onboarding",
    variation: "chrome",
    ua: "Google Chrome 123",
    dltoken: "00000000-0000-0000-0000-000000000000",
  });

  sandbox
    .stub(AddonRepository, "getAddonsByIDs")
    .resolves([
      { sourceURI: { scheme: "https", spec: "https://test.xpi" }, icons: {} },
    ]);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );
  registerCleanupFunction(async () => {
    sandbox.restore();
    BrowserTestUtils.removeTab(tab);
    // Clear cache call is only possible in a testing environment
    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    env.set("XPCSHELL_TEST_PROFILE_DIR", "testing");
    await ASRouter.forceAttribution({
      source: "",
      medium: "",
      campaign: "",
      content: "",
      experiment: "",
      variation: "",
      ua: "",
      dltoken: "",
    });
    // Clear and refresh Attribution, and then fetch the messages again to update
    AttributionCode._clearCache();
    await AttributionCode.getAttrDataAsync();
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

/**
 * Test the RTAMO welcome UI
 */
add_task(async function test_rtamo_aboutwelcome() {
  let browser = await openRTAMOWelcomePage();

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

  await onButtonClick(browser, "button.secondary");
  Assert.ok(gURLBar.focused, "Focus should be on awesome bar");

  let windowGlobalParent = browser.browsingContext.currentWindowGlobal;
  let aboutWelcomeActor = windowGlobalParent.getActor("AboutWelcome");
  const sandbox = sinon.createSandbox();
  // Stub AboutWelcomeParent Content Message Handler
  sandbox.stub(aboutWelcomeActor, "onContentMessage");
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await onButtonClick(browser, "button.primary");
  const { callCount } = aboutWelcomeActor.onContentMessage;
  ok(
    callCount === 2,
    `${callCount} Stub called twice to install extension and send telemetry`
  );

  const installExtensionCall = aboutWelcomeActor.onContentMessage.getCall(0);
  Assert.equal(
    installExtensionCall.args[0],
    "AWPage:SPECIAL_ACTION",
    "send special action to install add on"
  );
  Assert.equal(
    installExtensionCall.args[1].type,
    "INSTALL_ADDON_FROM_URL",
    "Special action type is INSTALL_ADDON_FROM_URL"
  );
  Assert.equal(
    installExtensionCall.args[1].data.url,
    "https://test.xpi",
    "Install add on url"
  );
  Assert.equal(
    installExtensionCall.args[1].data.telemetrySource,
    "rtamo",
    "Install add on telemetry source"
  );
  const telemetryCall = aboutWelcomeActor.onContentMessage.getCall(1);
  Assert.equal(
    telemetryCall.args[0],
    "AWPage:TELEMETRY_EVENT",
    "send add extension telemetry"
  );
  Assert.equal(
    telemetryCall.args[1].event,
    "INSTALL",
    "Telemetry event sent as INSTALL"
  );
  Assert.equal(
    telemetryCall.args[1].event_context.source,
    "ADD_EXTENSION_BUTTON",
    "Source of the event is Add Extension Button"
  );
  Assert.equal(
    telemetryCall.args[1].message_id,
    "RTAMO_DEFAULT_WELCOME",
    "Message Id sent in telemetry for default RTAMO"
  );
});
