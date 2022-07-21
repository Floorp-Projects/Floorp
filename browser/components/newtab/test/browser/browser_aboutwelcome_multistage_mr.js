"use strict";

const MR_TEMPLATE_PREF = "browser.aboutwelcome.templateMR";

async function openMRAboutWelcome() {
  await pushPrefs([MR_TEMPLATE_PREF, true]);
  await setAboutWelcomePref(true);
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
 * Test MR message telemetry
 */
add_task(async function test_aboutwelcome_mr_template_telemetry() {
  let browser = await openMRAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);

  let sandbox = sinon.createSandbox();

  // Stub AboutWelcomeParent's Content Message Handler
  const messageStub = sandbox.spy(aboutWelcomeActor, "onContentMessage");

  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await onButtonClick(browser, "button.secondary");

  const { callCount } = messageStub;
  ok(callCount >= 1, `${callCount} Stub was called`);
  let clickCall;
  for (let i = 0; i < callCount; i++) {
    const call = messageStub.getCall(i);
    info(`Call #${i}: ${call.args[0]} ${JSON.stringify(call.args[1])}`);
    if (call.calledWithMatch("", { event: "CLICK_BUTTON" })) {
      clickCall = call;
    }
  }

  Assert.ok(
    clickCall.args[1].message_id.startsWith("MR_WELCOME_DEFAULT"),
    "Telemetry includes MR message id"
  );
});

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
 * Test MR template content
 */
add_task(async function test_aboutwelcome_mr_template_content() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);
  let browser = await openMRAboutWelcome();

  await test_screen_content(
    browser,
    "MR template includes screens with split position",
    // Expected selectors:
    [`main.screen[pos="split"]`]
  );

  await test_screen_content(
    browser,
    "renders pin screen",
    //Expected selectors:
    ["main.AW_PIN_FIREFOX"],
    //Unexpected selectors:
    ["main.AW_GRATITUDE"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");

  //should render set default
  await test_screen_content(
    browser,
    "renders set default screen",
    //Expected selectors:
    ["main.AW_SET_DEFAULT"],
    //Unexpected selectors:
    ["main.AW_CHOOSE_THEME"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");

  await test_screen_content(
    browser,
    "renders import settings screen",
    //Expected selectors:
    ["main.AW_IMPORT_SETTINGS"],
    //Unexpected selectors:
    ["main.AW_PIN_FIREFOX"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");

  await test_screen_content(
    browser,
    "renders set colorway screen",
    //Expected selectors:
    ["main.AW_CHOOSE_THEME"],
    //Unexpected selectors:
    ["main.AW_PIN_FIREFOX"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");

  await test_screen_content(
    browser,
    "renders gratitude screen",
    //Expected selectors:
    ["main.AW_GRATITUDE"],
    //Unexpected selectors:
    ["main.AW_PIN_FIREFOX"]
  );
});

/**
 * Test MR template content - browser.shell.checkDefaultBrowser is false by default
 */
add_task(async function test_aboutwelcome_mr_template_content_default() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", false]);

  let browser = await openMRAboutWelcome();

  await test_screen_content(
    browser,
    "renders pin screen",
    //Expected selectors:
    ["main.AW_PIN_FIREFOX"],
    //Unexpected selectors:
    ["main.AW_GRATITUDE"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");

  await test_screen_content(
    browser,
    "renders set colorway screen",
    //Expected selectors:
    ["main.AW_CHOOSE_THEME"],
    //Unexpected selectors:
    ["main.AW_SET_DEFAULT"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");

  await test_screen_content(
    browser,
    "renders gratitude screen",
    //Expected selectors:
    ["main.AW_GRATITUDE"],
    //Unexpected selectors:
    ["main.AW_IMPORT_SETTINGS"]
  );
});
