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

  await onButtonClick(browser, ".action-buttons button.secondary");

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
 * Test MR template content
 */
add_task(async function test_aboutwelcome_mr_template_content() {
  let browser = await openMRAboutWelcome();

  await test_screen_content(
    browser,
    "MR template includes screens with split position",
    // Expected selectors:
    [`main.screen[pos="split"]`]
  );
});
