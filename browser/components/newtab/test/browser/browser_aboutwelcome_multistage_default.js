"use strict";

const DID_SEE_ABOUT_WELCOME_PREF = "trailhead.firstrun.didSeeAboutWelcome";
const MR_TEMPLATE_PREF = "browser.aboutwelcome.templateMR";

const TEST_PROTON_CONTENT = [
  {
    id: "AW_STEP1",
    content: {
      position: "corner",
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
      help_text: {
        text: "Here's some sample help text",
      },
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
    auto_advance: "primary_button",
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

const TEST_PROTON_JSON = JSON.stringify(TEST_PROTON_CONTENT);

async function openAboutWelcome() {
  await pushPrefs([MR_TEMPLATE_PREF, false]);
  await setAboutWelcomePref(true);
  await setAboutWelcomeMultiStage(TEST_PROTON_JSON);

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
 * Test the multistage welcome Proton UI
 */
add_task(async function test_multistage_aboutwelcome_proton() {
  const sandbox = sinon.createSandbox();
  let browser = await openAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);
  // Stub AboutWelcomeParent Content Message Handler
  sandbox.spy(aboutWelcomeActor, "onContentMessage");
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await test_screen_content(
    browser,
    "multistage proton step 1",
    // Expected selectors:
    [
      "main.AW_STEP1",
      "div.onboardingContainer",
      "div.proton[style*='chrome://activity-stream/content/data/content/assets']",
      "div.section-secondary",
      "span.attrib-text",
      "div.secondary-cta.top",
    ],
    // Unexpected selectors:
    [
      "main.AW_STEP2",
      "main.AW_STEP3",
      "main.dialog-initial",
      "main.dialog-last",
      "div.indicator.current",
    ]
  );

  // Ensure step indicator is not displayed
  await test_element_styles(
    browser,
    "div.steps",
    // Expected styles:
    {
      display: "none",
    }
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

  Assert.ok(
    clickCall.args[1].message_id === "DEFAULT_ABOUTWELCOME_PROTON_0_AW_STEP1",
    "AboutWelcome proton message id joined with screen id"
  );

  await test_screen_content(
    browser,
    "multistage proton step 2",
    // Expected selectors:
    [
      "main.AW_STEP2.dialog-initial",
      "div.onboardingContainer",
      "div.proton[style*='chrome://activity-stream/content/data/content/assets']",
      "div.section-main",
      "div.steps",
      "div.indicator.current",
    ],
    // Unexpected selectors:
    [
      "main.AW_STEP1",
      "main.AW_STEP3",
      "div.section-secondary",
      "main.dialog-last",
    ]
  );

  await onButtonClick(browser, "button.primary");

  // No 3rd screen to go to for win7.
  if (win7Content) return;

  await test_screen_content(
    browser,
    "multistage proton step 3",
    // Expected selectors:
    [
      "main.AW_STEP3",
      "div.onboardingContainer",
      "div.proton[style*='chrome://activity-stream/content/data/content/assets']",
      "div.section-main",
      "div.tiles-theme-container",
      "div.steps",
      "div.indicator.current",
    ],
    // Unexpected selectors:
    [
      "main.AW_STEP2",
      "main.AW_STEP1",
      "div.section-secondary",
      "main.dialog-initial",
      "main.dialog-last",
    ]
  );

  await onButtonClick(browser, "button.primary");

  await test_screen_content(
    browser,
    "multistage proton step 4",
    // Expected selectors:
    [
      "main.AW_STEP4.screen-1",
      "main.AW_STEP4.dialog-last",
      "div.onboardingContainer",
    ],
    // Unexpected selectors:
    [
      "main.AW_STEP2",
      "main.AW_STEP1",
      "main.AW_STEP3",
      "div.steps",
      "main.dialog-initial",
      "main.AW_STEP4.screen-0",
      "main.AW_STEP4.screen-2",
      "main.AW_STEP4.screen-3",
    ]
  );
});

/**
 * Test navigating back/forward between screens
 */
add_task(async function test_Multistage_About_Welcome_navigation() {
  let browser = await openAboutWelcome();

  await onButtonClick(browser, "button.primary");
  await TestUtils.waitForCondition(() => browser.canGoBack);
  browser.goBack();

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
    ],
    // Unexpected selectors:
    ["main.AW_STEP2", "main.AW_STEP3"]
  );

  await document.getElementById("forward-button").click();
});

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
  let performanceCall;
  for (let i = 0; i < callCount; i++) {
    const call = aboutWelcomeActor.onContentMessage.getCall(i);
    info(`Call #${i}: ${call.args[0]} ${JSON.stringify(call.args[1])}`);
    if (call.calledWithMatch("", { event: "CLICK_BUTTON" })) {
      clickCall = call;
    } else if (
      call.calledWithMatch("", {
        event_context: { importable: sinon.match.number },
      })
    ) {
      impressionCall = call;
    } else if (
      call.calledWithMatch("", {
        event_context: { mountStart: sinon.match.number },
      })
    ) {
      performanceCall = call;
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
      "DEFAULT_ABOUTWELCOME_PROTON_SITES",
      "SITES MessageId sent in impression event telemetry"
    );
    Assert.equal(
      impressionCall.args[1].event_context.page,
      "about:welcome",
      "event context page set to 'about:welcome'"
    );
  }

  // For some builds, we can stub fast enough to catch the performance
  if (performanceCall) {
    Assert.equal(
      performanceCall.args[0],
      "AWPage:TELEMETRY_EVENT",
      "send telemetry event"
    );
    Assert.equal(
      performanceCall.args[1].event,
      "IMPRESSION",
      "performance impression event recorded in telemetry"
    );
    Assert.equal(
      typeof performanceCall.args[1].event_context.domComplete,
      "number",
      "numeric domComplete recorded in telemetry"
    );
    Assert.equal(
      typeof performanceCall.args[1].event_context.domInteractive,
      "number",
      "numeric domInteractive recorded in telemetry"
    );
    Assert.equal(
      typeof performanceCall.args[1].event_context.mountStart,
      "number",
      "numeric mountStart recorded in telemetry"
    );
    Assert.equal(
      performanceCall.args[1].message_id,
      "DEFAULT_ABOUTWELCOME_PROTON",
      "MessageId sent in performance event telemetry"
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
    "DEFAULT_ABOUTWELCOME_PROTON_0_AW_STEP1",
    "MessageId sent in click event telemetry"
  );
});

add_task(async function test_AWMultistage_Secondary_Open_URL_Action() {
  if (win7Content) return;
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

  await onButtonClick(browser, "button[value='secondary_button_top']");
  const { callCount } = aboutWelcomeActor.onContentMessage;
  ok(
    callCount >= 2,
    `${callCount} Stub called twice to handle FxA open URL and Telemetry`
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
    "SHOW_FIREFOX_ACCOUNTS",
    "Special action SHOW_FIREFOX_ACCOUNTS event handled"
  );
  Assert.equal(
    actionCall.args[1].data.extraParams.utm_term,
    "aboutwelcome-default-screen",
    "UTMTerm set in FxA URL"
  );
  Assert.equal(
    actionCall.args[1].data.entrypoint,
    "test",
    "EntryPoint set in FxA URL"
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
    "secondary_button_top",
    "secondary_top button click source recorded in Telemetry"
  );
});

add_task(async function test_AWMultistage_Themes() {
  // No theme screen to test for win7.
  if (win7Content) return;

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
  await test_screen_content(
    browser,
    "multistage proton step 2",
    // Expected selectors:
    ["main.AW_STEP2.dialog-initial"],
    // Unexpected selectors:
    ["main.AW_STEP1"]
  );
  await onButtonClick(browser, "button.primary");

  await ContentTask.spawn(browser, "Themes", async () => {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector("label.theme"),
      "Theme Icons"
    );
    let themes = content.document.querySelectorAll("label.theme");
    Assert.equal(themes.length, 2, "Two themes displayed");
  });

  await onButtonClick(browser, "input[value=automatic]");

  const { callCount } = aboutWelcomeActor.onContentMessage;
  ok(callCount >= 1, `${callCount} Stub was called`);

  let actionCall;
  let eventCall;
  for (let i = 0; i < callCount; i++) {
    const call = aboutWelcomeActor.onContentMessage.getCall(i);
    info(`Call #${i}: ${call.args[0]} ${JSON.stringify(call.args[1])}`);
    if (call.calledWithMatch("SELECT_THEME")) {
      actionCall = call;
    } else if (call.calledWithMatch("", { event: "CLICK_BUTTON" })) {
      eventCall = call;
    }
  }

  Assert.equal(
    actionCall.args[0],
    "AWPage:SELECT_THEME",
    "Got call to handle select theme"
  );
  Assert.equal(
    actionCall.args[1],
    "AUTOMATIC",
    "Theme value passed as AUTOMATIC"
  );
  Assert.equal(
    eventCall.args[0],
    "AWPage:TELEMETRY_EVENT",
    "Got call to handle Telemetry event when theme tile clicked"
  );
  Assert.equal(
    eventCall.args[1].event,
    "CLICK_BUTTON",
    "click button event recorded in Telemetry"
  );
  Assert.equal(
    eventCall.args[1].event_context.source,
    "automatic",
    "automatic click source recorded in Telemetry"
  );
});

add_task(async function test_AWMultistage_can_restore_theme() {
  const { XPIProvider } = ChromeUtils.import(
    "resource://gre/modules/addons/XPIProvider.jsm"
  );
  const sandbox = sinon.createSandbox();
  registerCleanupFunction(() => sandbox.restore());

  const fakeAddons = [];
  class FakeAddon {
    constructor({ id = "default-theme@mozilla.org", isActive = false } = {}) {
      this.id = id;
      this.isActive = isActive;
    }
    enable() {
      for (let addon of fakeAddons) {
        addon.isActive = false;
      }
      this.isActive = true;
    }
  }
  fakeAddons.push(
    new FakeAddon({ id: "fake-theme-1@mozilla.org", isActive: true }),
    new FakeAddon({ id: "fake-theme-2@mozilla.org" })
  );

  let browser = await openAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);

  sandbox.stub(XPIProvider, "getAddonsByTypes").resolves(fakeAddons);
  sandbox
    .stub(XPIProvider, "getAddonByID")
    .callsFake(id => fakeAddons.find(addon => addon.id === id));
  sandbox.spy(aboutWelcomeActor, "onContentMessage");

  // Test that the active theme ID is stored in LIGHT_WEIGHT_THEMES
  await aboutWelcomeActor.receiveMessage({
    name: "AWPage:GET_SELECTED_THEME",
  });
  Assert.equal(
    await aboutWelcomeActor.onContentMessage.lastCall.returnValue,
    "automatic",
    `Should return "automatic" for non-built-in theme`
  );

  await aboutWelcomeActor.receiveMessage({
    name: "AWPage:SELECT_THEME",
    data: "AUTOMATIC",
  });
  Assert.equal(
    XPIProvider.getAddonByID.lastCall.args[0],
    fakeAddons[0].id,
    `LIGHT_WEIGHT_THEMES.AUTOMATIC should be ${fakeAddons[0].id}`
  );

  // Enable a different theme...
  fakeAddons[1].enable();
  // And test that AWGetSelectedTheme updates the active theme ID
  await aboutWelcomeActor.receiveMessage({
    name: "AWPage:GET_SELECTED_THEME",
  });
  await aboutWelcomeActor.receiveMessage({
    name: "AWPage:SELECT_THEME",
    data: "AUTOMATIC",
  });
  Assert.equal(
    XPIProvider.getAddonByID.lastCall.args[0],
    fakeAddons[1].id,
    `LIGHT_WEIGHT_THEMES.AUTOMATIC should be ${fakeAddons[1].id}`
  );
});

add_task(async function test_AWMultistage_Import() {
  // No import screen to test for win7.
  if (win7Content) return;
  let browser = await openAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);

  // Click twice to advance to screen 3
  await onButtonClick(browser, "button.primary");
  await test_screen_content(
    browser,
    "multistage proton step 2",
    // Expected selectors:
    ["main.AW_STEP2.dialog-initial"],
    // Unexpected selectors:
    ["main.AW_STEP1"]
  );
  await onButtonClick(browser, "button.primary");

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

  await test_screen_content(
    browser,
    "multistage proton step 2",
    // Expected selectors:
    ["main.AW_STEP3"],
    // Unexpected selectors:
    ["main.AW_STEP2"]
  );

  await onButtonClick(browser, "button[value='secondary_button']");
  const { callCount } = aboutWelcomeActor.onContentMessage;

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
    "SHOW_MIGRATION_WIZARD",
    "Special action SHOW_MIGRATION_WIZARD event handled"
  );
  Assert.equal(
    actionCall.args[1].data.source,
    "chrome",
    "Source passed to event handler"
  );
  Assert.equal(
    eventCall.args[0],
    "AWPage:TELEMETRY_EVENT",
    "Got call to handle Telemetry event"
  );
});

add_task(async function test_updatesPrefOnAWOpen() {
  Services.prefs.setBoolPref(DID_SEE_ABOUT_WELCOME_PREF, false);
  await setAboutWelcomePref(true);

  await openAboutWelcome();
  await TestUtils.waitForCondition(
    () =>
      Services.prefs.getBoolPref(DID_SEE_ABOUT_WELCOME_PREF, false) === true,
    "Updated pref to seen AW"
  );
  Services.prefs.clearUserPref(DID_SEE_ABOUT_WELCOME_PREF);
});

add_setup(async function() {
  const sandbox = sinon.createSandbox();
  // This needs to happen before any about:welcome page opens
  sandbox.stub(FxAccounts.config, "promiseMetricsFlowURI").resolves("");
  await setAboutWelcomeMultiStage("");

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

// Test Fxaccounts MetricsFlowURI
test_newtab(
  {
    async before({ pushPrefs }) {
      await pushPrefs(["browser.aboutwelcome.enabled", true]);
    },
    test: async function test_startBrowsing() {
      await ContentTaskUtils.waitForCondition(
        () => content.document.querySelector("div.onboardingContainer"),
        "Wait for about:welcome to load"
      );
    },
    after() {
      Assert.ok(
        FxAccounts.config.promiseMetricsFlowURI.called,
        "Stub was called"
      );
      Assert.equal(
        FxAccounts.config.promiseMetricsFlowURI.firstCall.args[0],
        "aboutwelcome",
        "Called by AboutWelcomeParent"
      );
    },
  },
  "about:welcome"
);

add_task(async function test_send_aboutwelcome_as_page_in_event_telemetry() {
  const sandbox = sinon.createSandbox();
  let browser = await openAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);
  // Stub AboutWelcomeParent Content Message Handler
  let telemetryStub = sandbox.stub(aboutWelcomeActor, "onContentMessage");

  await onButtonClick(browser, "button.primary");

  Assert.equal(
    telemetryStub.lastCall.args[1].event,
    "CLICK_BUTTON",
    "Event telemetry sent on primary button press"
  );
  Assert.equal(
    telemetryStub.lastCall.args[1].event_context.page,
    "about:welcome",
    "Event context page set to 'about:welcome' in event telemetry"
  );

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});
