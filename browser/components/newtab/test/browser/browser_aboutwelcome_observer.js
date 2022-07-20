"use strict";

const { AboutWelcomeParent } = ChromeUtils.import(
  "resource:///actors/AboutWelcomeParent.jsm"
);

async function openAboutWelcomeTab() {
  await setAboutWelcomePref(true);
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome"
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
  await setAboutWelcomePref(true);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    false
  );

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
  let windowGlobalParent =
    gBrowser.selectedBrowser.browsingContext.currentWindowGlobal;

  let aboutWelcomeActor = await windowGlobalParent.getActor("AboutWelcome");

  Assert.ok(
    aboutWelcomeActor.AboutWelcomeObserver,
    "AboutWelcomeObserver is not null"
  );
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "http://example.com/#foo");
  await BrowserTestUtils.waitForLocationChange(
    gBrowser,
    "http://example.com/#foo"
  );

  Assert.equal(
    aboutWelcomeActor.AboutWelcomeObserver.terminateReason,
    aboutWelcomeActor.AboutWelcomeObserver.AWTerminate.ADDRESS_BAR_NAVIGATED,
    "Terminated due to location uri changed"
  );
});
