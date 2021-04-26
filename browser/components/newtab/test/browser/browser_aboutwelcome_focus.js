"use strict";

const ABOUT_WELCOME_OVERRIDE_CONTENT_PREF = "browser.aboutwelcome.screens";
const ABOUT_WELCOME_FOCUS_PREF = "browser.aboutwelcome.skipFocus";

const TEST_MULTISTAGE_JSON = [
  {
    id: "AW_STEP1",
    order: 0,
    content: {
      title: "Step 1",
    },
  },
];

async function setAboutWelcomeOverrideContent(value) {
  return pushPrefs([ABOUT_WELCOME_OVERRIDE_CONTENT_PREF, value]);
}

async function openAboutWelcomeBrowserWindow() {
  let win = window.openDialog(
    AppConstants.BROWSER_CHROME_URL,
    "_blank",
    "chrome,all,dialog=no",
    "about:welcome"
  );

  await BrowserTestUtils.waitForEvent(win, "DOMContentLoaded");

  let promises = [
    BrowserTestUtils.firstBrowserLoaded(win, false),
    BrowserTestUtils.browserStopped(
      win.gBrowser.selectedBrowser,
      "about:welcome"
    ),
  ];

  await Promise.all(promises);

  await new Promise(resolve => {
    // 10 is an arbitrary value here, it needs to be at least 2 to avoid
    // races with code initializing itself using idle callbacks.
    (function waitForIdle(count = 10) {
      if (!count) {
        resolve();
        return;
      }
      Services.tm.idleDispatchToMainThread(() => {
        waitForIdle(count - 1);
      });
    })();
  });
  return win;
}

add_task(async function test_multistage_default() {
  let win = await openAboutWelcomeBrowserWindow();
  Assert.ok(!win.gURLBar.focused, "Focus should not be on awesome bar");
  Assert.ok(
    !win.gURLBar.hasAttribute("focused"),
    "No focused attribute on urlBar"
  );

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });
});

add_task(async function test_multistage_without_skipFocus() {
  await setAboutWelcomeOverrideContent(JSON.stringify(TEST_MULTISTAGE_JSON));
  await pushPrefs([ABOUT_WELCOME_FOCUS_PREF, false]);
  let win = await openAboutWelcomeBrowserWindow();
  Assert.ok(win.gURLBar.focused, "Focus should be on awesome bar");
  Assert.ok(
    win.gURLBar.hasAttribute("focused"),
    "Has focused attribute on urlBar"
  );

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });
});

add_task(async function test_multistage_with_skipFocus() {
  await setAboutWelcomeOverrideContent(
    JSON.stringify({ ...TEST_MULTISTAGE_JSON })
  );
  await pushPrefs([ABOUT_WELCOME_FOCUS_PREF, true]);
  let win = await openAboutWelcomeBrowserWindow();
  Assert.ok(!win.gURLBar.focused, "Focus should not be on awesome bar");
  Assert.ok(
    !win.gURLBar.hasAttribute("focused"),
    "No focused attribute on urlBar"
  );

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });
});
