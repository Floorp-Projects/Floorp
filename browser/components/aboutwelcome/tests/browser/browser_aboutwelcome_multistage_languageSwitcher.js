"use strict";

const { getAddonAndLocalAPIsMocker } = ChromeUtils.importESModule(
  "resource://testing-common/LangPackMatcherTestUtils.sys.mjs"
);

const { AWScreenUtils } = ChromeUtils.import(
  "resource:///modules/aboutwelcome/AWScreenUtils.jsm"
);

const sandbox = sinon.createSandbox();
const mockAddonAndLocaleAPIs = getAddonAndLocalAPIsMocker(this, sandbox);
add_task(function initSandbox() {
  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

/**
 * Spy specifically on the button click telemetry.
 *
 * The returned function flushes the spy of all of the matching button click events, and
 * returns the events.
 *
 * @returns {function(): TelemetryEvents[]}
 */
async function spyOnTelemetryButtonClicks(browser) {
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);
  sandbox.spy(aboutWelcomeActor, "onContentMessage");
  return () => {
    const result = aboutWelcomeActor.onContentMessage
      .getCalls()
      .filter(
        call =>
          call.args[0] === "AWPage:TELEMETRY_EVENT" &&
          call.args[1]?.event === "CLICK_BUTTON"
      )
      // The second argument is the telemetry event.
      .map(call => call.args[1]);

    aboutWelcomeActor.onContentMessage.resetHistory();
    return result;
  };
}

async function openAboutWelcome() {
  await pushPrefs(
    // Speed up the tests by disabling transitions.
    ["browser.aboutwelcome.transitions", false],
    ["intl.multilingual.aboutWelcome.languageMismatchEnabled", true]
  );
  await setAboutWelcomePref(true);

  sandbox
    .stub(AWScreenUtils, "evaluateScreenTargeting")
    .resolves(true)
    // Renders easy setup import screen as first screen to prevent pin/default dialog boxes breaking tests
    .withArgs(
      "doesAppNeedPin && 'browser.shell.checkDefaultBrowser'|preferenceValue && !isDefaultBrowser"
    )
    .resolves(false)
    .withArgs(
      "!doesAppNeedPin && 'browser.shell.checkDefaultBrowser'|preferenceValue && !isDefaultBrowser"
    )
    .resolves(false)
    .withArgs(
      "doesAppNeedPin && (!'browser.shell.checkDefaultBrowser'|preferenceValue || isDefaultBrowser)"
    )
    .resolves(false)
    .withArgs("isDeviceMigration")
    .resolves(false);

  info("Opening about:welcome");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );

  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
  });

  return {
    browser: tab.linkedBrowser,
    flushClickTelemetry: await spyOnTelemetryButtonClicks(tab.linkedBrowser),
  };
}

async function clickVisibleButton(browser, selector) {
  // eslint-disable-next-line no-shadow
  await ContentTask.spawn(browser, { selector }, async ({ selector }) => {
    function getVisibleElement() {
      for (const el of content.document.querySelectorAll(selector)) {
        if (el.offsetParent !== null) {
          return el;
        }
      }
      return null;
    }

    await ContentTaskUtils.waitForCondition(
      getVisibleElement,
      selector,
      200, // interval
      100 // maxTries
    );
    getVisibleElement().click();
  });
}

/**
 * Test that selectors are present and visible.
 */
async function testScreenContent(
  browser,
  name,
  expectedSelectors = [],
  unexpectedSelectors = []
) {
  await ContentTask.spawn(
    browser,
    { expectedSelectors, name, unexpectedSelectors },
    async ({
      expectedSelectors: expected,
      name: experimentName,
      unexpectedSelectors: unexpected,
    }) => {
      function selectorIsVisible(selector) {
        const els = content.document.querySelectorAll(selector);
        // The offsetParent will be null if element is hidden through "display: none;"
        return [...els].some(el => el.offsetParent !== null);
      }

      for (let selector of expected) {
        await ContentTaskUtils.waitForCondition(
          () => selectorIsVisible(selector),
          `Should render ${selector} in ${experimentName}`
        );
      }
      for (let selector of unexpected) {
        ok(
          !selectorIsVisible(selector),
          `Should not render ${selector} in ${experimentName}`
        );
      }
    }
  );
}

/**
 * Report telemetry mismatches nicely.
 */
function eventsMatch(
  actualEvents,
  expectedEvents,
  message = "Telemetry events match"
) {
  if (actualEvents.length !== expectedEvents.length) {
    console.error("Events do not match");
    console.error("Actual: ", JSON.stringify(actualEvents, null, 2));
    console.error("Expected: ", JSON.stringify(expectedEvents, null, 2));
  }
  for (let i = 0; i < actualEvents.length; i++) {
    const actualEvent = JSON.stringify(actualEvents[i], null, 2);
    const expectedEvent = JSON.stringify(expectedEvents[i], null, 2);
    if (actualEvent !== expectedEvent) {
      console.error("Events do not match");
      dump(`Actual: ${actualEvent}`);
      dump("\n");
      dump(`Expected: ${expectedEvent}`);
      dump("\n");
    }
    ok(actualEvent === expectedEvent, message);
  }
}

const liveLanguageSwitchSelectors = [
  ".screen.AW_LANGUAGE_MISMATCH",
  `[data-l10n-id*="onboarding-live-language"]`,
  `[data-l10n-id="mr2022-onboarding-live-language-text"]`,
];

/**
 * Accept the about:welcome offer to change the Firefox language when
 * there is a mismatch between the operating system language and the Firefox
 * language.
 */
add_task(async function test_aboutwelcome_languageSwitcher_accept() {
  sandbox.restore();
  const { resolveLangPacks, resolveInstaller, mockable } =
    mockAddonAndLocaleAPIs({
      systemLocale: "es-ES",
      appLocale: "en-US",
    });

  const { browser, flushClickTelemetry } = await openAboutWelcome();
  await testScreenContent(
    browser,
    "First Screen primary CTA loaded",
    // Expected selectors:
    [`button.primary[value="primary_button"]`],
    // Unexpected selectors:
    []
  );

  info("Clicking the primary button to start the onboarding process.");
  await clickVisibleButton(browser, `button.primary[value="primary_button"]`);

  await testScreenContent(
    browser,
    "Live language switching (waiting for languages)",
    // Expected selectors:
    [
      ...liveLanguageSwitchSelectors,
      `[data-l10n-id="mr2022-onboarding-live-language-text"]`,
      `button[disabled] [data-l10n-id="onboarding-live-language-waiting-button"]`,
      `[data-l10n-id="mr2022-onboarding-secondary-skip-button-label"]`,
    ],
    // Unexpected selectors:
    []
  );

  // Ignore the telemetry of the initial welcome screen.
  flushClickTelemetry();

  resolveLangPacks(["es-MX", "es-ES", "fr-FR"]);

  await testScreenContent(
    browser,
    "Live language switching, asking for a language",
    // Expected selectors:
    [
      ...liveLanguageSwitchSelectors,
      `button.primary[value="primary_button"]`,
      `button.primary[value="decline"]`,
    ],
    // Unexpected selectors:
    [
      `button[disabled] [data-l10n-id="mr2022-onboarding-live-language-waiting-button"]`,
      `[data-l10n-id="mr2022-onboarding-secondary-skip-button-label"]`,
    ]
  );

  info("Clicking the primary button to view language switching page.");

  await clickVisibleButton(browser, "button.primary");

  await testScreenContent(
    browser,
    "Live language switching, waiting for langpack to download",
    // Expected selectors:
    [
      ...liveLanguageSwitchSelectors,
      `[data-l10n-id="onboarding-live-language-button-label-downloading"]`,
      `[data-l10n-id="onboarding-live-language-secondary-cancel-download"]`,
    ],
    // Unexpected selectors:
    [
      `button[disabled] [data-l10n-id="onboarding-live-language-waiting-button"]`,
    ]
  );

  eventsMatch(flushClickTelemetry(), [
    {
      event: "CLICK_BUTTON",
      event_context: {
        source: "download_langpack",
        page: "about:welcome",
      },
      message_id: "MR_WELCOME_DEFAULT_1_AW_LANGUAGE_MISMATCH",
    },
  ]);

  sinon.assert.notCalled(mockable.setRequestedAppLocales);

  await resolveInstaller();

  await testScreenContent(
    browser,
    "Language changed",
    // Expected selectors:
    [`.screen.AW_IMPORT_SETTINGS_EMBEDDED`],
    // Unexpected selectors:
    liveLanguageSwitchSelectors
  );

  info("The app locale was changed to the OS locale.");
  sinon.assert.calledWith(mockable.setRequestedAppLocales, ["es-ES", "en-US"]);

  eventsMatch(flushClickTelemetry(), [
    {
      event: "CLICK_BUTTON",
      event_context: {
        source: "download_complete",
        page: "about:welcome",
      },
      message_id: "MR_WELCOME_DEFAULT_1_AW_LANGUAGE_MISMATCH",
    },
  ]);
});

/**
 * Test declining the about:welcome offer to change the Firefox language when
 * there is a mismatch between the operating system language and the Firefox
 * language.
 */
add_task(async function test_aboutwelcome_languageSwitcher_decline() {
  sandbox.restore();
  const { resolveLangPacks, resolveInstaller, mockable } =
    mockAddonAndLocaleAPIs({
      systemLocale: "es-ES",
      appLocale: "en-US",
    });

  const { browser, flushClickTelemetry } = await openAboutWelcome();
  await testScreenContent(
    browser,
    "First Screen primary CTA loaded",
    // Expected selectors:
    [`button.primary[value="primary_button"]`],
    // Unexpected selectors:
    []
  );

  info("Clicking the primary button to view language switching page.");
  await clickVisibleButton(browser, `button.primary[value="primary_button"]`);

  await testScreenContent(
    browser,
    "Live language switching (waiting for languages)",
    // Expected selectors:
    [
      ...liveLanguageSwitchSelectors,
      `[data-l10n-id="mr2022-onboarding-live-language-text"]`,
      `button[disabled] [data-l10n-id="onboarding-live-language-waiting-button"]`,
      `[data-l10n-id="mr2022-onboarding-secondary-skip-button-label"]`,
    ],
    // Unexpected selectors:
    []
  );

  // Ignore the telemetry of the initial welcome screen.
  flushClickTelemetry();

  resolveLangPacks(["es-MX", "es-ES", "fr-FR"]);
  resolveInstaller();

  await testScreenContent(
    browser,
    "Live language switching, asking for a language",
    // Expected selectors:
    [
      ...liveLanguageSwitchSelectors,
      `button.primary[value="primary_button"]`,
      `button.primary[value="decline"]`,
    ],
    // Unexpected selectors:
    [
      `button[disabled] [data-l10n-id="onboarding-live-language-waiting-button"]`,
      `[data-l10n-id="mr2022-onboarding-secondary-skip-button-label"]`,
    ]
  );

  sinon.assert.notCalled(mockable.setRequestedAppLocales);

  info("Clicking the secondary button to skip installing the langpack.");
  await clickVisibleButton(browser, `button.primary[value="decline"]`);

  await testScreenContent(
    browser,
    "Language selection declined",
    // Expected selectors:
    [`.screen.AW_IMPORT_SETTINGS_EMBEDDED`],
    // Unexpected selectors:
    liveLanguageSwitchSelectors
  );

  info("The requested locale should be set to the original en-US");
  sinon.assert.calledWith(mockable.setRequestedAppLocales, ["en-US"]);

  eventsMatch(flushClickTelemetry(), [
    {
      event: "CLICK_BUTTON",
      event_context: {
        source: "decline",
        page: "about:welcome",
      },
      message_id: "MR_WELCOME_DEFAULT_1_AW_LANGUAGE_MISMATCH",
    },
  ]);
});

/**
 * Ensure the langpack can be installed before the user gets to the language screen.
 */
add_task(async function test_aboutwelcome_languageSwitcher_asyncCalls() {
  sandbox.restore();
  const { resolveLangPacks, resolveInstaller, mockable } =
    mockAddonAndLocaleAPIs({
      systemLocale: "es-ES",
      appLocale: "en-US",
    });

  await openAboutWelcome();

  info("Waiting for getAvailableLangpacks to be called.");
  await TestUtils.waitForCondition(
    () => mockable.getAvailableLangpacks.called,
    "getAvailableLangpacks called once"
  );
  ok(mockable.installLangPack.notCalled);

  resolveLangPacks(["es-MX", "es-ES", "fr-FR"]);

  await TestUtils.waitForCondition(
    () => mockable.installLangPack.called,
    "installLangPack was called once"
  );
  ok(mockable.getAvailableLangpacks.called);

  resolveInstaller();
});

/**
 * Test that the "en-US" langpack is installed, if it's already available as the last
 * fallback locale.
 */
add_task(async function test_aboutwelcome_fallback_locale() {
  sandbox.restore();
  const { resolveLangPacks, resolveInstaller, mockable } =
    mockAddonAndLocaleAPIs({
      systemLocale: "en-US",
      appLocale: "it",
    });

  await openAboutWelcome();

  info("Waiting for getAvailableLangpacks to be called.");
  await TestUtils.waitForCondition(
    () => mockable.getAvailableLangpacks.called,
    "getAvailableLangpacks called once"
  );
  ok(mockable.installLangPack.notCalled);

  resolveLangPacks(["en-US"]);

  await TestUtils.waitForCondition(
    () => mockable.installLangPack.called,
    "installLangPack was called once"
  );
  ok(mockable.getAvailableLangpacks.called);

  resolveInstaller();
});

/**
 * Test when AMO does not have a matching language.
 */
add_task(async function test_aboutwelcome_languageSwitcher_noMatch() {
  sandbox.restore();
  const { resolveLangPacks, mockable } = mockAddonAndLocaleAPIs({
    systemLocale: "tlh", // Klingon
    appLocale: "en-US",
  });

  const { browser } = await openAboutWelcome();

  info("Clicking the primary button to start installing the langpack.");
  await clickVisibleButton(browser, `button.primary[value="primary_button"]`);

  // Klingon is not supported.
  resolveLangPacks(["es-MX", "es-ES", "fr-FR"]);

  await testScreenContent(
    browser,
    "Language selection skipped",
    // Expected selectors:
    [`.screen.AW_IMPORT_SETTINGS_EMBEDDED`],
    // Unexpected selectors:
    [
      `[data-l10n-id*="onboarding-live-language"]`,
      `[data-l10n-id="onboarding-live-language-header"]`,
    ]
  );
  sinon.assert.notCalled(mockable.setRequestedAppLocales);
});

/**
 * Test when bidi live reloading is not supported.
 */
add_task(async function test_aboutwelcome_languageSwitcher_bidiNotSupported() {
  sandbox.restore();
  await pushPrefs(["intl.multilingual.liveReloadBidirectional", false]);

  const { mockable } = mockAddonAndLocaleAPIs({
    systemLocale: "ar-EG", // Arabic (Egypt)
    appLocale: "en-US",
  });

  const { browser } = await openAboutWelcome();

  info("Clicking the primary button to start installing the langpack.");
  await clickVisibleButton(browser, `button.primary[value="primary_button"]`);

  await testScreenContent(
    browser,
    "Language selection skipped for bidi",
    // Expected selectors:
    [`.screen.AW_IMPORT_SETTINGS_EMBEDDED`],
    // Unexpected selectors:
    [
      `[data-l10n-id*="onboarding-live-language"]`,
      `[data-l10n-id="onboarding-live-language-header"]`,
    ]
  );

  sinon.assert.notCalled(mockable.setRequestedAppLocales);
});

/**
 * Test when bidi live reloading is not supported and no langpacks.
 */
add_task(
  async function test_aboutwelcome_languageSwitcher_bidiNotSupported_noLangPacks() {
    sandbox.restore();
    await pushPrefs(["intl.multilingual.liveReloadBidirectional", false]);

    const { resolveLangPacks, mockable } = mockAddonAndLocaleAPIs({
      systemLocale: "ar-EG", // Arabic (Egypt)
      appLocale: "en-US",
    });
    resolveLangPacks([]);

    const { browser } = await openAboutWelcome();

    info("Clicking the primary button to start installing the langpack.");
    await clickVisibleButton(browser, `button.primary[value="primary_button"]`);

    await testScreenContent(
      browser,
      "Language selection skipped for bidi",
      // Expected selectors:
      [`.screen.AW_IMPORT_SETTINGS_EMBEDDED`],
      // Unexpected selectors:
      [
        `[data-l10n-id*="onboarding-live-language"]`,
        `[data-l10n-id="onboarding-live-language-header"]`,
      ]
    );

    sinon.assert.notCalled(mockable.setRequestedAppLocales);
  }
);

/**
 * Test when bidi live reloading is supported.
 */
add_task(async function test_aboutwelcome_languageSwitcher_bidiNotSupported() {
  sandbox.restore();
  await pushPrefs(["intl.multilingual.liveReloadBidirectional", true]);

  const { resolveLangPacks, mockable } = mockAddonAndLocaleAPIs({
    systemLocale: "ar-EG", // Arabic (Egypt)
    appLocale: "en-US",
  });

  const { browser } = await openAboutWelcome();

  info("Clicking the primary button to start installing the langpack.");
  await clickVisibleButton(browser, `button.primary[value="primary_button"]`);

  resolveLangPacks(["ar-EG", "es-ES", "fr-FR"]);

  await testScreenContent(
    browser,
    "Live language switching with bidi supported",
    // Expected selectors:
    [...liveLanguageSwitchSelectors],
    // Unexpected selectors:
    []
  );

  sinon.assert.notCalled(mockable.setRequestedAppLocales);
});

/**
 * Test hitting the cancel button when waiting on a langpack.
 */
add_task(async function test_aboutwelcome_languageSwitcher_cancelWaiting() {
  sandbox.restore();
  const { resolveLangPacks, resolveInstaller, mockable } =
    mockAddonAndLocaleAPIs({
      systemLocale: "es-ES",
      appLocale: "en-US",
    });

  const { browser, flushClickTelemetry } = await openAboutWelcome();

  info("Clicking the primary button to start the onboarding process.");
  await clickVisibleButton(browser, `button.primary[value="primary_button"]`);
  resolveLangPacks(["es-MX", "es-ES", "fr-FR"]);

  await testScreenContent(
    browser,
    "Live language switching, asking for a language",
    // Expected selectors:
    liveLanguageSwitchSelectors,
    // Unexpected selectors:
    []
  );

  info("Clicking the primary button to view language switching page.");
  await clickVisibleButton(browser, `button.primary[value="primary_button"]`);

  await testScreenContent(
    browser,
    "Live language switching, waiting for langpack to download",
    // Expected selectors:
    [
      ...liveLanguageSwitchSelectors,
      `[data-l10n-id="onboarding-live-language-button-label-downloading"]`,
      `[data-l10n-id="onboarding-live-language-secondary-cancel-download"]`,
    ],
    // Unexpected selectors:
    [
      `button[disabled] [data-l10n-id="onboarding-live-language-waiting-button"]`,
    ]
  );

  // Ignore all the telemetry up to this point.
  flushClickTelemetry();

  info("Cancel the request for the language");
  await clickVisibleButton(browser, "button.secondary");

  await testScreenContent(
    browser,
    "Language selection declined waiting",
    // Expected selectors:
    [`.screen.AW_IMPORT_SETTINGS_EMBEDDED`],
    // Unexpected selectors:
    liveLanguageSwitchSelectors
  );

  eventsMatch(flushClickTelemetry(), [
    {
      event: "CLICK_BUTTON",
      event_context: {
        source: "cancel_waiting",
        page: "about:welcome",
      },
      message_id: "MR_WELCOME_DEFAULT_1_AW_LANGUAGE_MISMATCH",
    },
  ]);

  await resolveInstaller();

  is(flushClickTelemetry().length, 0);
  sinon.assert.notCalled(mockable.setRequestedAppLocales);
});

/**
 * Test MR About Welcome language mismatch screen
 */
add_task(async function test_aboutwelcome_languageSwitcher_MR() {
  sandbox.restore();

  const { resolveLangPacks, resolveInstaller } = mockAddonAndLocaleAPIs({
    systemLocale: "es-ES",
    appLocale: "en-US",
  });

  const { browser } = await openAboutWelcome(true);

  info("Clicking the primary button to view language switching screen.");
  await clickVisibleButton(browser, `button.primary[value="primary_button"]`);

  resolveLangPacks(["es-AR"]);
  await testScreenContent(
    browser,
    "Live language switching, asking for a language",
    // Expected selectors:
    [
      `#mainContentHeader[data-l10n-id="mr2022-onboarding-live-language-text"]`,
      `[data-l10n-id="mr2022-language-mismatch-subtitle"]`,
      `.section-secondary [data-l10n-id="mr2022-onboarding-live-language-text"]`,
      `[data-l10n-id="mr2022-onboarding-live-language-switch-to"]`,
      `button.primary[value="primary_button"]`,
      `button.primary[value="decline"]`,
    ],
    // Unexpected selectors:
    [`[data-l10n-id="onboarding-live-language-header"]`]
  );

  await resolveInstaller();
  await testScreenContent(
    browser,
    "Switched some to langpack (raw) strings after install",
    // Expected selectors:
    [`#mainContentHeader[data-l10n-id="mr2022-onboarding-live-language-text"]`],
    // Unexpected selectors:
    [
      `.section-secondary [data-l10n-id="mr2022-onboarding-live-language-text"]`,
      `[data-l10n-id="mr2022-onboarding-live-language-switch-to"]`,
    ]
  );
});
