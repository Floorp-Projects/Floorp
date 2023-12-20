"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const { AboutWelcomeTelemetry } = ChromeUtils.import(
  "resource:///modules/aboutwelcome/AboutWelcomeTelemetry.jsm"
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

async function testAboutWelcomeLogoFor(logo = {}) {
  info(`Testing logo: ${JSON.stringify(logo)}`);

  let screens = [makeTestContent("TEST_LOGO_SELECTION_STEP", { logo })];

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    value: { enabled: true, screens },
  });

  let browser = await openAboutWelcome(JSON.stringify(screens));

  let expected = [
    `.brand-logo[src="${
      logo.imageURL ?? "chrome://branding/content/about-logo.svg"
    }"][alt=""]`,
  ];
  let unexpected = [];
  (logo.alt ? unexpected : expected).push('.brand-logo[role="presentation"]');
  (logo.width ? expected : unexpected).push(`.brand-logo[style*="width"]`);
  (logo.height ? expected : unexpected).push(`.brand-logo[style*="height"]`);
  (logo.marginBlock ? expected : unexpected).push(
    `.logo-container[style*="margin-block"]`
  );
  (logo.marginInline ? expected : unexpected).push(
    `.logo-container[style*="margin-inline"]`
  );
  (logo.darkModeImageURL ? expected : unexpected).push(
    `.logo-container source[media="(prefers-color-scheme: dark)"]${
      logo.darkModeImageURL ? `[srcset="${logo.darkModeImageURL}"]` : ""
    }`
  );
  (logo.reducedMotionImageURL ? expected : unexpected).push(
    `.logo-container source[media="(prefers-reduced-motion: reduce)"]${
      logo.reducedMotionImageURL
        ? `[srcset="${logo.reducedMotionImageURL}"]`
        : ""
    }`
  );
  (logo.darkModeReducedMotionImageURL ? expected : unexpected).push(
    `.logo-container source[media="(prefers-color-scheme: dark) and (prefers-reduced-motion: reduce)"]${
      logo.darkModeReducedMotionImageURL
        ? `[srcset="${logo.darkModeReducedMotionImageURL}"]`
        : ""
    }`
  );
  await test_screen_content(
    browser,
    "renders screen with passed logo",
    expected,
    unexpected
  );

  await doExperimentCleanup();
  browser.closeBrowser();
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
  browser.closeBrowser();
});

/**
 * Test rendering a screen with a customized logo
 */
add_task(async function test_aboutwelcome_with_customized_logo() {
  const TEST_LOGO_URL = "chrome://branding/content/icon64.png";
  const TEST_LOGO_HEIGHT = "50px";
  const TEST_LOGO_CONTENT = makeTestContent("TEST_LOGO_STEP", {
    logo: {
      height: TEST_LOGO_HEIGHT,
      imageURL: TEST_LOGO_URL,
    },
  });
  const TEST_LOGO_JSON = JSON.stringify([TEST_LOGO_CONTENT]);
  let browser = await openAboutWelcome(TEST_LOGO_JSON);

  await test_screen_content(
    browser,
    "renders screen with customized logo",
    // Expected selectors:
    ["main.TEST_LOGO_STEP[pos='center']", `.brand-logo[src="${TEST_LOGO_URL}"]`]
  );

  // Ensure logo has custom height
  await test_element_styles(
    browser,
    ".brand-logo",
    // Expected styles:
    {
      // Override default text-link styles
      height: TEST_LOGO_HEIGHT,
    }
  );
  browser.closeBrowser();
});

/**
 * Test rendering a screen with empty logo used for padding
 */
add_task(async function test_aboutwelcome_with_empty_logo_spacing() {
  const TEST_LOGO_HEIGHT = "50px";
  const TEST_LOGO_CONTENT = makeTestContent("TEST_LOGO_STEP", {
    logo: {
      height: TEST_LOGO_HEIGHT,
      imageURL: "none",
    },
  });
  const TEST_LOGO_JSON = JSON.stringify([TEST_LOGO_CONTENT]);
  let browser = await openAboutWelcome(TEST_LOGO_JSON);

  await test_screen_content(
    browser,
    "renders screen with empty logo element",
    // Expected selectors:
    ["main.TEST_LOGO_STEP[pos='center']", ".brand-logo[src='none']"]
  );

  // Ensure logo has custom height
  await test_element_styles(
    browser,
    ".brand-logo",
    // Expected styles:
    {
      // Override default text-link styles
      height: TEST_LOGO_HEIGHT,
    }
  );
  browser.closeBrowser();
});

/**
 * Test rendering a screen with a "Title Logo"
 */
add_task(async function test_title_logo() {
  const TEST_TITLE_LOGO_URL = "chrome://branding/content/icon64.png";
  const TEST_CONTENT = makeTestContent("TITLE_LOGO", {
    title_logo: {
      imageURL: TEST_TITLE_LOGO_URL,
    },
  });
  const TEST_JSON = JSON.stringify([TEST_CONTENT]);
  let browser = await openAboutWelcome(TEST_JSON);

  await test_screen_content(
    browser,
    "renders screen with title_logo",
    // Expected selectors:
    [".inline-icon-container"]
  );

  browser.closeBrowser();
});

/**
 * Test rendering a screen with a title with custom styles.
 */
add_task(async function test_aboutwelcome_with_title_styles() {
  const TEST_TITLE_STYLE_CONTENT = makeTestContent("TEST_TITLE_STYLE_STEP", {
    title: {
      fontSize: "36px",
      fontWeight: 276,
      letterSpacing: 0,
      raw: "test",
    },
    title_style: "fancy shine",
  });

  const TEST_TITLE_STYLE_JSON = JSON.stringify([TEST_TITLE_STYLE_CONTENT]);
  let browser = await openAboutWelcome(TEST_TITLE_STYLE_JSON);

  await test_screen_content(
    browser,
    "renders screen with customized title style",
    // Expected selectors:
    [`div.welcome-text.fancy.shine`]
  );

  await test_element_styles(
    browser,
    "#mainContentHeader",
    // Expected styles:
    {
      "font-weight": "276",
      "font-size": "36px",
      animation: "50s linear 0s infinite normal none running shine",
      "letter-spacing": "normal",
    }
  );
  browser.closeBrowser();
});

/**
 * Test rendering a screen with an image for the dialog window's background
 */
add_task(async function test_aboutwelcome_with_background() {
  const BACKGROUND_URL =
    "chrome://activity-stream/content/data/content/assets/confetti.svg";
  const TEST_BACKGROUND_CONTENT = makeTestContent("TEST_BACKGROUND_STEP", {
    background: `url(${BACKGROUND_URL}) no-repeat center/cover`,
  });

  const TEST_BACKGROUND_JSON = JSON.stringify([TEST_BACKGROUND_CONTENT]);
  let browser = await openAboutWelcome(TEST_BACKGROUND_JSON);

  await test_screen_content(
    browser,
    "renders screen with dialog background image",
    // Expected selectors:
    [`div.main-content[style*='${BACKGROUND_URL}'`]
  );
  browser.closeBrowser();
});

/**
 * Test rendering a screen with defined dimensions
 */
add_task(async function test_aboutwelcome_with_dimensions() {
  const TEST_DIMENSIONS_CONTENT = makeTestContent("TEST_DIMENSIONS_STEP", {
    width: "100px",
    position: "center",
  });

  const TEST_DIMENSIONS_JSON = JSON.stringify([TEST_DIMENSIONS_CONTENT]);
  let browser = await openAboutWelcome(TEST_DIMENSIONS_JSON);

  await test_screen_content(
    browser,
    "renders screen with defined dimensions",
    // Expected selectors:
    [`div.main-content[style*='width: 100px;']`]
  );
  browser.closeBrowser();
});

/**
 * Test rendering a screen with a dismiss button
 */
add_task(async function test_aboutwelcome_dismiss_button() {
  let browser = await openAboutWelcome(
    JSON.stringify(
      // Use 2 screens to test that the message is dismissed, not navigated
      [1, 2].map(i =>
        makeTestContent(`TEST_DISMISS_STEP_${i}`, {
          dismiss_button: { action: { dismiss: true }, size: "small" },
        })
      )
    )
  );

  await test_screen_content(
    browser,
    "renders screen with dismiss button",
    // Expected selectors:
    ['div.section-main button.dismiss-button[button-size="small"]']
  );

  // Click dismiss button
  await onButtonClick(browser, "button.dismiss-button");

  // Wait for about:home to load
  await BrowserTestUtils.browserLoaded(browser, false, "about:home");
  is(browser.currentURI.spec, "about:home", "about:home loaded");

  browser.closeBrowser();
});

/**
 * Test rendering a screen with the "split" position
 */
add_task(async function test_aboutwelcome_split_position() {
  const TEST_SPLIT_STEP = makeTestContent("TEST_SPLIT_STEP", {
    position: "split",
    hero_text: "hero test",
  });

  const TEST_SPLIT_JSON = JSON.stringify([TEST_SPLIT_STEP]);
  let browser = await openAboutWelcome(TEST_SPLIT_JSON);

  await test_screen_content(
    browser,
    "renders screen secondary section containing hero text",
    // Expected selectors:
    [`main.screen[pos="split"]`, `.section-secondary`, `.message-text h1`]
  );

  // Ensure secondary section has split template styling
  await test_element_styles(
    browser,
    "main.screen .section-secondary",
    // Expected styles:
    {
      display: "flex",
      margin: "auto 0px auto auto",
    }
  );

  // Ensure secondary action has button styling
  await test_element_styles(
    browser,
    ".action-buttons .secondary-cta .secondary",
    // Expected styles:
    {
      // Override default text-link styles
      "background-color": "color(srgb 0.0823529 0.0784314 0.101961 / 0.07)",
      color: "rgb(21, 20, 26)",
    }
  );
  browser.closeBrowser();
});

/**
 * Test rendering a screen with a URL value and default color for backdrop
 */
add_task(async function test_aboutwelcome_with_url_backdrop() {
  const TEST_BACKDROP_URL = `url("chrome://activity-stream/content/data/content/assets/confetti.svg")`;
  const TEST_BACKDROP_VALUE = `#212121 ${TEST_BACKDROP_URL} center/cover no-repeat fixed`;
  const TEST_URL_BACKDROP_CONTENT = makeTestContent("TEST_URL_BACKDROP_STEP");

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    value: {
      enabled: true,
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
  browser.closeBrowser();
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
    value: {
      enabled: true,
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
  browser.closeBrowser();
});

/**
 * Test rendering a screen with a text color override
 */
add_task(async function test_aboutwelcome_with_text_color_override() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Override the system color scheme to dark
      ["ui.systemUsesDarkTheme", 1],
    ],
  });

  let screens = [];
  // we need at least two screens to test the step indicator
  for (let i = 0; i < 2; i++) {
    screens.push(
      makeTestContent("TEST_TEXT_COLOR_OVERRIDE_STEP", {
        text_color: "dark",
        background: "white",
      })
    );
  }

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    value: {
      enabled: true,
      screens,
    },
  });
  let browser = await openAboutWelcome(JSON.stringify(screens));

  await test_screen_content(
    browser,
    "renders screen with dark text",
    // Expected selectors:
    [`main.screen.dark-text`, `.indicator.current`, `.indicator:not(.current)`],
    // Unexpected selectors:
    [`main.screen.light-text`]
  );

  // Ensure title inherits light text color
  await test_element_styles(
    browser,
    "#mainContentHeader",
    // Expected styles:
    {
      color: "rgb(21, 20, 26)",
    }
  );

  // Ensure next step indicator inherits light color
  await test_element_styles(
    browser,
    ".indicator:not(.current)",
    // Expected styles:
    {
      color: "rgb(251, 251, 254)",
    }
  );

  await doExperimentCleanup();
  await SpecialPowers.popPrefEnv();
  browser.closeBrowser();
});

/**
 * Test rendering a screen with a "progress bar" style step indicator
 */
add_task(async function test_aboutwelcome_with_progress_bar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["ui.systemUsesDarkTheme", 0],
      ["ui.prefersReducedMotion", 0],
    ],
  });
  let screens = [];
  // we need at least three screens to test the progress bar styling
  for (let i = 0; i < 3; i++) {
    screens.push(
      makeTestContent(`TEST_MR_PROGRESS_BAR_${i + 1}`, {
        position: "split",
        progress_bar: true,
        primary_button: {
          label: "next",
          action: {
            navigate: true,
          },
        },
      })
    );
  }

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    value: {
      enabled: true,
      screens,
    },
  });
  let browser = await openAboutWelcome(JSON.stringify(screens));

  await SpecialPowers.spawn(browser, [], async () => {
    const progressBar = await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(".progress-bar")
    );
    const indicator = await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(".indicator")
    );
    // Progress bar should have a gray background.
    is(
      content.window.getComputedStyle(progressBar)["background-color"],
      "color(srgb 0.0823529 0.0784314 0.101961 / 0.25)",
      "Correct progress bar background"
    );

    const indicatorStyles = content.window.getComputedStyle(indicator);
    for (let [key, val] of Object.entries({
      // The filled "completed" element should have
      // `background-color: var(--in-content-primary-button-background);`
      "background-color": "rgb(0, 97, 224)",
      // Base progress bar step styles.
      height: "6px",
      "margin-inline": "-1px",
      "padding-block": "0px",
    })) {
      is(indicatorStyles[key], val, `Correct indicator ${key} style`);
    }
    const indicatorX = indicator.getBoundingClientRect().x;
    content.document.querySelector("button.primary").click();
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector(".indicator")?.getBoundingClientRect()
          .x > indicatorX,
      "Indicator should have grown"
    );
  });

  await doExperimentCleanup();
  browser.closeBrowser();
});

/**
 * Test rendering a message with session history updates disabled
 */
add_task(async function test_aboutwelcome_history_updates_disabled() {
  let screens = [];
  // we need at least two screens to test the history state
  for (let i = 1; i < 3; i++) {
    screens.push(makeTestContent(`TEST_PUSH_STATE_STEP_${i}`));
  }
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    value: {
      enabled: true,
      disableHistoryUpdates: true,
      screens,
    },
  });
  let browser = await openAboutWelcome(JSON.stringify(screens));

  let startHistoryLength = await SpecialPowers.spawn(browser, [], () => {
    return content.window.history.length;
  });
  // Advance to second screen
  await onButtonClick(browser, "button.primary");
  let endHistoryLength = await SpecialPowers.spawn(browser, [], async () => {
    // Ensure next screen has rendered
    await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(".TEST_PUSH_STATE_STEP_2")
    );
    return content.window.history.length;
  });

  ok(
    startHistoryLength === endHistoryLength,
    "No entries added to the session's history stack with history updates disabled"
  );

  await doExperimentCleanup();
  browser.closeBrowser();
});

/**
 * Test rendering a screen with different logos depending on reduced motion and
 * color scheme preferences
 */
add_task(async function test_aboutwelcome_logo_selection() {
  // Test a screen config that includes every logo parameter
  await testAboutWelcomeLogoFor({
    imageURL: "chrome://branding/content/icon16.png",
    darkModeImageURL: "chrome://branding/content/icon32.png",
    reducedMotionImageURL: "chrome://branding/content/icon64.png",
    darkModeReducedMotionImageURL: "chrome://branding/content/icon128.png",
    alt: "TEST_LOGO_SELECTION_ALT",
    width: "16px",
    height: "16px",
    marginBlock: "0px",
    marginInline: "0px",
  });
  // Test a screen config with no animated/static logos
  await testAboutWelcomeLogoFor({
    imageURL: "chrome://branding/content/icon16.png",
    darkModeImageURL: "chrome://branding/content/icon32.png",
  });
  // Test a screen config with no dark mode logos
  await testAboutWelcomeLogoFor({
    imageURL: "chrome://branding/content/icon16.png",
    reducedMotionImageURL: "chrome://branding/content/icon64.png",
  });
  // Test a screen config that includes only the default logo
  await testAboutWelcomeLogoFor({
    imageURL: "chrome://branding/content/icon16.png",
  });
  // Test a screen config with no logos
  await testAboutWelcomeLogoFor();
});

/**
 * Test rendering a message that starts on a specific screen
 */
add_task(async function test_aboutwelcome_start_screen_configured() {
  let startScreen = 1;
  let screens = [];
  // we need at least two screens to test
  for (let i = 1; i < 3; i++) {
    screens.push(makeTestContent(`TEST_START_STEP_${i}`));
  }
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    value: {
      enabled: true,
      startScreen,
      screens,
    },
  });

  let sandbox = sinon.createSandbox();
  let spy = sandbox.spy(AboutWelcomeTelemetry.prototype, "sendTelemetry");

  let browser = await openAboutWelcome(JSON.stringify(screens));

  let secondScreenShown = await SpecialPowers.spawn(browser, [], async () => {
    // Ensure screen has rendered
    await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(".TEST_START_STEP_2")
    );
    return true;
  });

  ok(
    secondScreenShown,
    `Starts on second screen when configured with startScreen index equal to ${startScreen}`
  );
  // Wait for screen elements to render before checking impression pings
  await test_screen_content(
    browser,
    "renders second screen elements",
    // Expected selectors:
    [`main.screen`, "div.secondary-cta"]
  );

  let expectedTelemetry = sinon.match({
    event: "IMPRESSION",
    message_id: `MR_WELCOME_DEFAULT_${startScreen}_TEST_START_STEP_${
      startScreen + 1
    }_${screens.map(({ id }) => id?.split("_")[1]?.[0]).join("")}`,
  });
  if (spy.calledWith(expectedTelemetry)) {
    ok(
      true,
      "Impression events have the correct message id with start screen configured"
    );
  } else if (spy.called) {
    ok(
      false,
      `Wrong telemetry sent: ${JSON.stringify(
        spy.getCalls().map(c => c.args[0]),
        null,
        2
      )}`
    );
  } else {
    ok(false, "No telemetry sent");
  }

  await doExperimentCleanup();
  browser.closeBrowser();
  sandbox.restore();
});
