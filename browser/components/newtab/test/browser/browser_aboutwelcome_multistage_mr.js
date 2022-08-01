"use strict";

const MR_TEMPLATE_PREF = "browser.aboutwelcome.templateMR";

const { AboutWelcomeParent } = ChromeUtils.import(
  "resource:///actors/AboutWelcomeParent.jsm"
);

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

const sandbox = sinon.createSandbox();
let isDefaultStub;
let pinStub;

add_task(function initSandbox() {
  pinStub = sandbox.stub(AboutWelcomeParent, "doesAppNeedPin").returns(true);
  isDefaultStub = sandbox
    .stub(AboutWelcomeParent, "isDefaultBrowser")
    .returns(false);

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

/**
 * Test MR message telemetry
 */
add_task(async function test_aboutwelcome_mr_template_telemetry() {
  let browser = await openMRAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);

  // Stub AboutWelcomeParent's Content Message Handler
  const messageStub = sandbox.spy(aboutWelcomeActor, "onContentMessage");

  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await clickVisibleButton(browser, "button.secondary");

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

/**
 * Test MR template content - Browser is not Pinned and not set as default
 */
add_task(async function test_aboutwelcome_mr_template_content() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);

  pinStub.returns(true);
  isDefaultStub.returns(false);

  let browser = await openMRAboutWelcome();

  await test_screen_content(
    browser,
    "MR template includes screens with split position and a sign in link on the first screen",
    // Expected selectors:
    [
      `main.screen[pos="split"]`,
      "div.secondary-cta.top",
      "button[value='secondary_button_top']",
    ]
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
});

/**
 * Test MR template content - Browser has been set as Default, not pinned
 */
add_task(async function test_aboutwelcome_mr_template_content_pin() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);

  pinStub.returns(true);
  isDefaultStub.returns(true);

  let browser = await openMRAboutWelcome();

  await test_screen_content(
    browser,
    "renders pin screen",
    //Expected selectors:
    ["main.AW_PIN_FIREFOX"],
    //Unexpected selectors:
    ["main.AW_SET_DEFAULT"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");

  await test_screen_content(
    browser,
    "renders next screen",
    //Expected selectors:
    ["main"],
    //Unexpected selectors:
    ["main.AW_SET_DEFAULT"]
  );
});

/**
 * Test MR template content - Browser is Pinned, not default
 */
add_task(async function test_aboutwelcome_mr_template_only_default() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);

  pinStub.returns(false);
  isDefaultStub.returns(false);

  let browser = await openMRAboutWelcome();

  //should render set default
  await test_screen_content(
    browser,
    "renders set default screen",
    //Expected selectors:
    ["main.AW_ONLY_DEFAULT"],
    //Unexpected selectors:
    ["main.AW_PIN_FIREFOX"]
  );
});
/**
 * Test MR template content - Browser is Pinned and set as default
 */
add_task(async function test_aboutwelcome_mr_template_get_started() {
  await pushPrefs(["browser.shell.checkDefaultBrowser", true]);

  pinStub.returns(false);
  isDefaultStub.returns(true);

  let browser = await openMRAboutWelcome();

  //should render set default
  await test_screen_content(
    browser,
    "doesn't render pin and set default screens",
    //Expected selectors:
    ["main.AW_GET_STARTED"],
    //Unexpected selectors:
    ["main.AW_PIN_FIREFOX", "main.AW_ONLY_DEFAULT"]
  );
  sandbox.restore();
});
