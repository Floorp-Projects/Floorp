"use strict";

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

const TEST_PROTON_JSON = JSON.stringify(TEST_PROTON_CONTENT);

/**
 * Test the multistage welcome Proton UI
 */
add_task(async function test_multistage_aboutwelcome_proton() {
  const sandbox = sinon.createSandbox();
  await setAboutWelcomePref(true);
  await setAboutWelcomeMultiStage(TEST_PROTON_JSON);
  await setProton(true);

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
    [
      "main.AW_STEP1",
      "div.onboardingContainer",
      "div.proton[style*='.jpg']",
      "div.section-left",
      "span.attrib-text",
      "div.secondary-cta.top",
    ],
    // Unexpected selectors:
    [
      "main.AW_STEP2",
      "main.AW_STEP3",
      "nav.steps",
      "main.dialog-initial",
      "main.dialog-last",
    ]
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
      "div.proton[style*='.jpg']",
      "div.section-main",
      "nav.steps",
    ],
    // Unexpected selectors:
    ["main.AW_STEP1", "main.AW_STEP3", "div.section-left", "main.dialog-last"]
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
      "div.proton[style*='.jpg']",
      "div.section-main",
      "div.tiles-theme-container",
      "nav.steps",
    ],
    // Unexpected selectors:
    [
      "main.AW_STEP2",
      "main.AW_STEP1",
      "div.section-left",
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
      "main.dialog-initial",
      "main.AW_STEP4.screen-0",
      "main.AW_STEP4.screen-2",
      "main.AW_STEP4.screen-3",
    ]
  );
});
