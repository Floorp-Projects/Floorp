const { OnboardingMessageProvider } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/OnboardingMessageProvider.sys.mjs"
);
const { ToolbarBadgeHub } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ToolbarBadgeHub.sys.mjs"
);

add_task(async function test_setup() {
  // Cleanup pref value because we click the fxa accounts button.
  // This is not required during tests because we "force show" the message
  // by sending it directly to the Hub bypassing targeting.
  registerCleanupFunction(() => {
    // Clicking on the Firefox Accounts button while in the signed out
    // state opens a new tab for signing in.
    // We'll clean those up here for now.
    gBrowser.removeAllTabsBut(gBrowser.tabs[0]);
    // Stop the load in the last tab that remains.
    gBrowser.stop();
    Services.prefs.clearUserPref("identity.fxaccounts.toolbar.accessed");
  });
});

add_task(async function test_fxa_badge_shown_nodelay() {
  const [msg] = (await OnboardingMessageProvider.getMessages()).filter(
    ({ id }) => id === "FXA_ACCOUNTS_BADGE"
  );

  Assert.ok(msg, "FxA test message exists");

  // Ensure we badge immediately
  msg.content.delay = undefined;

  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  // Click the button and clear the badge that occurs normally at startup
  let fxaButton = browserWindow.document.getElementById(msg.content.target);
  fxaButton.click();

  await BrowserTestUtils.waitForCondition(
    () =>
      !browserWindow.document
        .getElementById(msg.content.target)
        .querySelector(".toolbarbutton-badge")
        .classList.contains("feature-callout"),
    "Initially element is not badged"
  );

  ToolbarBadgeHub.registerBadgeNotificationListener(msg);

  await BrowserTestUtils.waitForCondition(
    () =>
      browserWindow.document
        .getElementById(msg.content.target)
        .querySelector(".toolbarbutton-badge")
        .classList.contains("feature-callout"),
    "Wait for element to be badged"
  );

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  browserWindow = Services.wm.getMostRecentWindow("navigator:browser");

  await BrowserTestUtils.waitForCondition(
    () =>
      browserWindow.document
        .getElementById(msg.content.target)
        .querySelector(".toolbarbutton-badge")
        .classList.contains("feature-callout"),
    "Wait for element to be badged"
  );

  await BrowserTestUtils.closeWindow(newWin);
  browserWindow = Services.wm.getMostRecentWindow("navigator:browser");

  // Click the button and clear the badge
  fxaButton = document.getElementById(msg.content.target);
  fxaButton.click();

  await BrowserTestUtils.waitForCondition(
    () =>
      !browserWindow.document
        .getElementById(msg.content.target)
        .querySelector(".toolbarbutton-badge")
        .classList.contains("feature-callout"),
    "Button should no longer be badged"
  );
});

add_task(async function test_fxa_badge_shown_withdelay() {
  const [msg] = (await OnboardingMessageProvider.getMessages()).filter(
    ({ id }) => id === "FXA_ACCOUNTS_BADGE"
  );

  Assert.ok(msg, "FxA test message exists");

  // Enough to trigger the setTimeout badging
  msg.content.delay = 1;

  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  // Click the button and clear the badge that occurs normally at startup
  let fxaButton = browserWindow.document.getElementById(msg.content.target);
  fxaButton.click();

  await BrowserTestUtils.waitForCondition(
    () =>
      !browserWindow.document
        .getElementById(msg.content.target)
        .querySelector(".toolbarbutton-badge")
        .classList.contains("feature-callout"),
    "Initially element is not badged"
  );

  ToolbarBadgeHub.registerBadgeNotificationListener(msg);

  await BrowserTestUtils.waitForCondition(
    () =>
      browserWindow.document
        .getElementById(msg.content.target)
        .querySelector(".toolbarbutton-badge")
        .classList.contains("feature-callout"),
    "Wait for element to be badged"
  );

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  browserWindow = Services.wm.getMostRecentWindow("navigator:browser");

  await BrowserTestUtils.waitForCondition(
    () =>
      browserWindow.document
        .getElementById(msg.content.target)
        .querySelector(".toolbarbutton-badge")
        .classList.contains("feature-callout"),
    "Wait for element to be badged"
  );

  await BrowserTestUtils.closeWindow(newWin);
  browserWindow = Services.wm.getMostRecentWindow("navigator:browser");

  // Click the button and clear the badge
  fxaButton = document.getElementById(msg.content.target);
  fxaButton.click();

  await BrowserTestUtils.waitForCondition(
    () =>
      !browserWindow.document
        .getElementById(msg.content.target)
        .querySelector(".toolbarbutton-badge")
        .classList.contains("feature-callout"),
    "Button should no longer be badged"
  );
});
