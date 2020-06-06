"use strict";

const SEPARATE_ABOUT_WELCOME_PREF = "browser.aboutwelcome.enabled";
const ABOUT_WELCOME_OVERRIDE_CONTENT_PREF =
  "browser.aboutwelcome.overrideContent";

const TEST_MULTISTAGE_CONTENT = {
  id: "multi-stage-welcome",
  screens: [
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
          label: "link top",
          position: "top",
          action: {
            type: "OPEN_URL",
            data: {
              args: "http://example.com/",
            },
          },
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
          position: "bottom",
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
      },
    },
  ],
};
const TEST_MULTISTAGE_JSON = JSON.stringify(TEST_MULTISTAGE_CONTENT);
/**
 * Sets the aboutwelcome pref to enabled simplified welcome UI
 */
async function setAboutWelcomePref(value) {
  return pushPrefs([SEPARATE_ABOUT_WELCOME_PREF, value]);
}

async function setAboutWelcomeMultiStage(value) {
  return pushPrefs([ABOUT_WELCOME_OVERRIDE_CONTENT_PREF, value]);
}

async function openAboutWelcome() {
  await setAboutWelcomePref(true);
  await setAboutWelcomeMultiStage(TEST_MULTISTAGE_JSON);

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
 * Setup and test simplified welcome UI
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

      if (experimentName === "home") {
        Assert.equal(
          content.document.location.href,
          "about:home",
          "Navigated to about:home"
        );
      } else {
        Assert.equal(
          content.document.location.href,
          "about:welcome",
          "Navigated to a welcome screen"
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
 * Test the multistage welcome UI rendered using TEST_MULTISTAGE_JSON
 */
add_task(async function test_Multistage_About_Welcome_branches() {
  let browser = await openAboutWelcome();

  await test_screen_content(
    browser,
    "multistage step 1",
    // Expected selectors:
    [
      "div.multistageContainer",
      "main.AW_STEP1",
      "div.secondary-cta.top",
      "button.secondary",
      "div.indicator.current",
    ],
    // Unexpected selectors:
    ["main.AW_STEP2", "main.AW_STEP3"]
  );

  await onButtonClick(browser, "button.primary");
  await test_screen_content(
    browser,
    "multistage step 2",
    // Expected selectors:
    ["div.multistageContainer", "main.AW_STEP2", "button.secondary"],
    // Unexpected selectors:
    ["main.AW_STEP1", "main.AW_STEP3", "div.secondary-cta.top"]
  );
  await onButtonClick(browser, "button.primary");
  await test_screen_content(
    browser,
    "multistage step 3",
    // Expected selectors:
    [
      "div.multistageContainer",
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
    ["div.multistageContainer"]
  );
});

async function getAboutWelcomeParent(browser) {
  let windowGlobalParent = browser.browsingContext.currentWindowGlobal;
  return windowGlobalParent.getActor("AboutWelcome");
}

/**
 * Test the multistage welcome UI primary button action
 */
add_task(async function test_AWMultistage_Primary_Action() {
  let browser = await openAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);
  const sandbox = sinon.createSandbox();
  // Stub AboutWelcomeParent Content Message Handler
  sandbox
    .stub(aboutWelcomeActor, "onContentMessage")
    .resolves("")
    .withArgs("AWPage:IMPORTABLE_SITES")
    .resolves([]);
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await onButtonClick(browser, "button.primary");
  const { callCount } = aboutWelcomeActor.onContentMessage;
  ok(callCount >= 1, `${callCount} Stub was called`);

  let clickCall;
  let impressionCall;
  for (let i = 0; i < callCount; i++) {
    const call = aboutWelcomeActor.onContentMessage.getCall(i);
    info(`Call #${i}: ${call.args[0]} ${JSON.stringify(call.args[1])}`);
    if (call.calledWithMatch("", { event: "CLICK_BUTTON" })) {
      clickCall = call;
    } else if (call.calledWithMatch("", { event: "IMPRESSION" })) {
      impressionCall = call;
    }
  }

  // For some builds, we can stub fast enough to catch the impression
  if (impressionCall) {
    Assert.equal(
      impressionCall.args[0],
      "AWPage:TELEMETRY_EVENT",
      "send telemetry event"
    );
    Assert.equal(
      impressionCall.args[1].event,
      "IMPRESSION",
      "impression event recorded in telemetry"
    );
    Assert.equal(
      impressionCall.args[1].event_context.display,
      "static",
      "static sites display recorded in telemetry"
    );
    Assert.equal(
      typeof impressionCall.args[1].event_context.importable,
      "number",
      "numeric importable sites recorded in telemetry"
    );
    Assert.equal(
      impressionCall.args[1].message_id,
      `${TEST_MULTISTAGE_CONTENT.id}_SITES`.toUpperCase(),
      "SITES MessageId sent in impression event telemetry"
    );
  }

  Assert.equal(
    clickCall.args[0],
    "AWPage:TELEMETRY_EVENT",
    "send telemetry event"
  );
  Assert.equal(
    clickCall.args[1].event,
    "CLICK_BUTTON",
    "click button event recorded in telemetry"
  );
  Assert.equal(
    clickCall.args[1].event_context.source,
    "primary_button",
    "primary button click source recorded in telemetry"
  );
  Assert.equal(
    clickCall.args[1].message_id,
    `${TEST_MULTISTAGE_CONTENT.id}_${TEST_MULTISTAGE_CONTENT.screens[0].id}`.toUpperCase(),
    "MessageId sent in click event telemetry"
  );
});

add_task(async function test_AWMultistage_Secondary_Open_URL_Action() {
  let browser = await openAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);
  const sandbox = sinon.createSandbox();
  // Stub AboutWelcomeParent Content Message Handler
  sandbox
    .stub(aboutWelcomeActor, "onContentMessage")
    .resolves("")
    .withArgs("AWPage:IMPORTABLE_SITES")
    .resolves([]);
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await onButtonClick(browser, "button.secondary");
  const { callCount } = aboutWelcomeActor.onContentMessage;
  ok(
    callCount >= 2,
    `${callCount} Stub called twice to handle Open_URL and Telemetry`
  );

  let actionCall;
  let eventCall;
  for (let i = 0; i < callCount; i++) {
    const call = aboutWelcomeActor.onContentMessage.getCall(i);
    info(`Call #${i}: ${call.args[0]} ${JSON.stringify(call.args[1])}`);
    if (call.calledWithMatch("SPECIAL")) {
      actionCall = call;
    } else if (call.calledWithMatch("", { event: "CLICK_BUTTON" })) {
      eventCall = call;
    }
  }

  Assert.equal(
    actionCall.args[0],
    "AWPage:SPECIAL_ACTION",
    "Got call to handle special action"
  );
  Assert.equal(
    actionCall.args[1].type,
    "OPEN_URL",
    "Special action OPEN_URL event handled"
  );
  ok(
    actionCall.args[1].data.args.includes(
      "utm_term=aboutwelcome-default-screen"
    ),
    "UTMTerm set in opened URL"
  );

  Assert.equal(
    eventCall.args[0],
    "AWPage:TELEMETRY_EVENT",
    "Got call to handle Telemetry event"
  );
  Assert.equal(
    eventCall.args[1].event,
    "CLICK_BUTTON",
    "click button event recorded in Telemetry"
  );
  Assert.equal(
    eventCall.args[1].event_context.source,
    "secondary_button",
    "secondary button click source recorded in Telemetry"
  );
});
