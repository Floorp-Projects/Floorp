/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

import { BrowserTestUtils } from "resource://testing-common/BrowserTestUtils.sys.mjs";
import { Assert } from "resource://testing-common/Assert.sys.mjs";
import { TestUtils } from "resource://testing-common/TestUtils.sys.mjs";

function assertFirefoxViewTab(win) {
  Assert.ok(win.FirefoxViewHandler.tab, "Firefox View tab exists");
  Assert.ok(win.FirefoxViewHandler.tab?.hidden, "Firefox View tab is hidden");
  Assert.equal(
    win.gBrowser.visibleTabs.indexOf(win.FirefoxViewHandler.tab),
    -1,
    "Firefox View tab is not in the list of visible tabs"
  );
}

async function assertFirefoxViewTabSelected(win) {
  assertFirefoxViewTab(win);
  Assert.ok(
    win.FirefoxViewHandler.tab.selected,
    "Firefox View tab is selected"
  );
  await BrowserTestUtils.browserLoaded(
    win.FirefoxViewHandler.tab.linkedBrowser
  );
}

async function openFirefoxViewTab(win) {
  Assert.ok(
    !win.FirefoxViewHandler.tab,
    "Firefox View tab doesn't exist prior to clicking the button"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#firefox-view-button",
    { type: "mousedown" },
    win.browsingContext
  );
  assertFirefoxViewTab(win);
  Assert.ok(
    win.FirefoxViewHandler.tab.selected,
    "Firefox View tab is selected"
  );
  await BrowserTestUtils.browserLoaded(
    win.FirefoxViewHandler.tab.linkedBrowser
  );
  return win.FirefoxViewHandler.tab;
}

function closeFirefoxViewTab(win) {
  win.gBrowser.removeTab(win.FirefoxViewHandler.tab);
  Assert.ok(
    !win.FirefoxViewHandler.tab,
    "Reference to Firefox View tab got removed when closing the tab"
  );
}

async function withFirefoxView(
  { resetFlowManager = true, win = null },
  taskFn
) {
  let shouldCloseWin = false;
  if (!win) {
    win = await BrowserTestUtils.openNewBrowserWindow();
    shouldCloseWin = true;
  }
  if (resetFlowManager) {
    const { TabsSetupFlowManager } = ChromeUtils.importESModule(
      "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
    );
    // reset internal state so we aren't reacting to whatever state the last invocation left behind
    TabsSetupFlowManager.resetInternalState();
  }
  // Setting this pref allows the test to run as expected with a keyboard on MacOS
  await win.SpecialPowers.pushPrefEnv({
    set: [["accessibility.tabfocus", 7]],
  });
  let tab = await openFirefoxViewTab(win);
  let originalWindow = tab.ownerGlobal;
  let result = await taskFn(tab.linkedBrowser);
  let finalWindow = tab.ownerGlobal;
  if (originalWindow == finalWindow && !tab.closing && tab.linkedBrowser) {
    // taskFn may resolve within a tick after opening a new tab.
    // We shouldn't remove the newly opened tab in the same tick.
    // Wait for the next tick here.
    await TestUtils.waitForTick();
    BrowserTestUtils.removeTab(tab);
  } else {
    Services.console.logStringMessage(
      "withFirefoxView: Tab was already closed before " +
        "removeTab would have been called"
    );
  }
  await win.SpecialPowers.popPrefEnv();
  if (shouldCloseWin) {
    await BrowserTestUtils.closeWindow(win);
  }
  return result;
}

export {
  withFirefoxView,
  assertFirefoxViewTab,
  assertFirefoxViewTabSelected,
  openFirefoxViewTab,
  closeFirefoxViewTab,
};
