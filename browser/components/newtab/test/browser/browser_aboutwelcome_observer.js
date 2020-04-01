"use strict";

const SEPARATE_ABOUT_WELCOME_PREF = "browser.aboutwelcome.enabled";
const { AboutWelcomeParent } = ChromeUtils.import(
  "resource:///actors/AboutWelcomeParent.jsm"
);

/**
 * Sets the aboutwelcome pref to enabled simplified welcome UI
 */
async function setAboutWelcomePref(value) {
  return pushPrefs([SEPARATE_ABOUT_WELCOME_PREF, value]);
}

async function openAboutWelcomeTab() {
  await setAboutWelcomePref(true);
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    false
  );
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
  });
  return tab;
}

/**
 * Test simplified welcome UI tab closed terminate reason
 */
add_task(async function test_About_Welcome_Tab_Close() {
  let tab = await openAboutWelcomeTab();

  Assert.ok(Services.focus.activeWindow, "Active window is not null");
  let AWP = new AboutWelcomeParent();
  Assert.ok(AWP.AboutWelcomeObserver, "AboutWelcomeObserver is not null");

  BrowserTestUtils.removeTab(tab);
  Assert.equal(
    AWP.AboutWelcomeObserver.terminateReason,
    AWP.AboutWelcomeObserver.AWTerminate.TAB_CLOSED,
    "Terminated due to tab closed"
  );
});

/**
 * Test simplified welcome UI closed due to change in location uri
 */
add_task(async function test_About_Welcome_Location_Change() {
  await openAboutWelcomeTab();

  let AWP = new AboutWelcomeParent();
  Assert.ok(AWP.AboutWelcomeObserver, "AboutWelcomeObserver is not null");

  await BrowserTestUtils.loadURI(
    gBrowser.selectedBrowser,
    "http://example.com/#foo"
  );
  await BrowserTestUtils.waitForLocationChange(
    gBrowser,
    "http://example.com/#foo"
  );

  Assert.equal(
    AWP.AboutWelcomeObserver.terminateReason,
    AWP.AboutWelcomeObserver.AWTerminate.ADDRESS_BAR_NAVIGATED,
    "Terminated due to location uri changed"
  );
});

add_task(async function test_About_Welcome_On_AppShutDown() {
  await openAboutWelcomeTab();
  let AWP = new AboutWelcomeParent();
  Assert.ok(AWP.AboutWelcomeObserver, "AboutWelcomeObserver is not null");

  Services.obs.notifyObservers(null, "quit-application");
  Assert.equal(
    AWP.AboutWelcomeObserver.terminateReason,
    AWP.AboutWelcomeObserver.AWTerminate.APP_SHUT_DOWN,
    "Terminated due to app shut down"
  );
});

async function openAboutWelcomeWindow() {
  await setAboutWelcomePref(true);
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let tab = BrowserTestUtils.addTab(win.gBrowser, "about:welcome");

  registerCleanupFunction(() => {
    if (tab && tab.ownerGlobal) {
      BrowserTestUtils.removeTab(tab);
    }
  });
  return win;
}

/**
 * Test simplified welcome UI terminate due to window closed
 */
add_task(async function test_About_Welcome_Window_Closed() {
  let win = await openAboutWelcomeWindow();

  let AWP = new AboutWelcomeParent();
  Assert.ok(AWP.AboutWelcomeObserver, "AboutWelcomeObserver is not null");

  await BrowserTestUtils.closeWindow(win);
  Assert.equal(
    AWP.AboutWelcomeObserver.terminateReason,
    AWP.AboutWelcomeObserver.AWTerminate.WINDOW_CLOSED,
    "Terminated due to window closed"
  );
});
