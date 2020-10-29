"use strict";

const ABOUT_WELCOME_OVERRIDE_CONTENT_PREF =
  "browser.aboutwelcome.overrideContent";

const TEST_RTAMO_WELCOME_CONTENT = {
  template: "return_to_amo",
  name: "Test add on",
  url: "https://test.xpi",
  iconURL: "https://test.svg",
  content: {
    header: { string_id: "onboarding-welcome-header" },
    subtitle: { string_id: "return-to-amo-subtitle" },
    text: {
      string_id: "return-to-amo-addon-title",
    },
    primary_button: {
      label: { string_id: "return-to-amo-add-extension-label" },
      action: {
        type: "INSTALL_ADDON_FROM_URL",
        data: { url: null, telemetrySource: "rtamo" },
      },
    },
    startButton: {
      label: {
        string_id: "onboarding-start-browsing-button-label",
      },
      message_id: "RTAMO_START_BROWSING_BUTTON",
      action: {
        type: "OPEN_AWESOME_BAR",
      },
    },
  },
};

const TEST_RTAMO_WELCOME_JSON = JSON.stringify(TEST_RTAMO_WELCOME_CONTENT);

async function setAboutWelcomeOverride(value) {
  return pushPrefs([ABOUT_WELCOME_OVERRIDE_CONTENT_PREF, value]);
}

async function openRTAMOWelcomePage() {
  await setAboutWelcomeOverride(TEST_RTAMO_WELCOME_JSON);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
    pushPrefs([ABOUT_WELCOME_OVERRIDE_CONTENT_PREF, ""]);
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
