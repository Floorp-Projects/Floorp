/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Tests that the status panel gets cleared when entering DOM fullscreen
 * (bug 1850993), and that we don't show network statuses in DOM fullscreen
 * (bug 1853896). */

// DOM FS tests tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

let statuspanel = document.getElementById("statuspanel");
let statuspanelLabel = document.getElementById("statuspanel-label");

async function withDomFsTab(beforeEnter, afterEnter) {
  let url = "https://example.com/";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    url,
    true,
    true
  );
  let browser = tab.linkedBrowser;

  await beforeEnter();
  info("Entering DOM fullscreen");
  await changeFullscreen(browser, true);
  is(document.fullscreenElement, browser, "Entered DOM fullscreen");
  await afterEnter();

  await BrowserTestUtils.removeTab(tab);
}

add_task(async function test_overlink() {
  const overlink = "https://example.com";
  let setAndCheckOverLink = async info => {
    XULBrowserWindow.setOverLink(overlink);
    await TestUtils.waitForCondition(
      () => BrowserTestUtils.isVisible(statuspanel),
      `statuspanel should become visible after setting overlink ${info}`
    );
    is(
      statuspanelLabel.value,
      BrowserUIUtils.trimURL(overlink),
      `statuspanel has expected value after setting overlink ${info}`
    );
  };
  await withDomFsTab(
    async function () {
      await setAndCheckOverLink("outside of DOM FS");
    },
    async function () {
      await TestUtils.waitForCondition(
        () => !BrowserTestUtils.isVisible(statuspanel),
        "statuspanel with overlink should hide when entering DOM FS"
      );
      await setAndCheckOverLink("while in DOM FS");
    }
  );
});

add_task(async function test_networkstatus() {
  await withDomFsTab(
    async function () {
      XULBrowserWindow.status = "test1";
      XULBrowserWindow.busyUI = true;
      StatusPanel.update();
      ok(
        BrowserTestUtils.isVisible(statuspanel),
        "statuspanel is visible before entering DOM FS"
      );
      is(statuspanelLabel.value, "test1", "statuspanel has expected value");
    },
    async function () {
      is(
        XULBrowserWindow.busyUI,
        true,
        "browser window still considered busy (i.e. loading stuff) when entering DOM FS"
      );
      is(
        XULBrowserWindow.status,
        "",
        "network status cleared when entering DOM FS"
      );
      ok(
        !BrowserTestUtils.isVisible(statuspanel),
        "statuspanel with network status should should hide when entering DOM FS"
      );
    }
  );
});
