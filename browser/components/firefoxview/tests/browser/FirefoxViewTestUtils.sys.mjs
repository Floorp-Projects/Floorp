/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

import { BrowserTestUtils } from "resource://testing-common/BrowserTestUtils.sys.mjs";
import { Assert } from "resource://testing-common/Assert.sys.mjs";
import { TestUtils } from "resource://testing-common/TestUtils.sys.mjs";

var testScope;

/**
 * Module consumers can optionally initialize the module
 *
 * @param {object} scope
 *   object with SimpleTest and info properties.
 */
function init(scope) {
  testScope = scope;
}

function getFirefoxViewURL() {
  return Services.prefs.getBoolPref("browser.tabs.firefox-view-next", true)
    ? "about:firefoxview-next"
    : "about:firefoxview";
}

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
  if (!testScope?.SimpleTest) {
    throw new Error(
      "Must initialize FirefoxViewTestUtils with a test scope which has a SimpleTest property"
    );
  }
  await testScope.SimpleTest.promiseFocus(win);
  let fxviewTab = win.FirefoxViewHandler.tab;
  let alreadyLoaded =
    fxviewTab?.linkedBrowser.currentURI.spec.includes(getFirefoxViewURL()) &&
    fxviewTab?.linkedBrowser?.contentDocument?.readyState == "complete";
  let enteredPromise = alreadyLoaded
    ? Promise.resolve()
    : TestUtils.topicObserved("firefoxview-entered");

  if (!fxviewTab?.selected) {
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#firefox-view-button",
      { type: "mousedown" },
      win.browsingContext
    );
    await TestUtils.waitForTick();
  }

  fxviewTab = win.FirefoxViewHandler.tab;
  assertFirefoxViewTab(win);
  Assert.ok(
    win.FirefoxViewHandler.tab.selected,
    "Firefox View tab is selected"
  );

  testScope.info(
    "openFirefoxViewTab, waiting for complete readyState, visible and firefoxview-entered"
  );
  await Promise.all([
    TestUtils.waitForCondition(() => {
      const document = fxviewTab.linkedBrowser.contentDocument;
      return (
        document.readyState == "complete" &&
        document.visibilityState == "visible"
      );
    }),
    enteredPromise,
  ]);
  testScope.info("openFirefoxViewTab, ready resolved");
  return fxviewTab;
}

function closeFirefoxViewTab(win) {
  if (win.FirefoxViewHandler.tab) {
    win.gBrowser.removeTab(win.FirefoxViewHandler.tab);
  }
  Assert.ok(
    !win.FirefoxViewHandler.tab,
    "Reference to Firefox View tab got removed when closing the tab"
  );
}

/**
 * Run a task with Firefox View open.
 *
 * @param {object} options
 *   Options object.
 * @param {boolean} [options.openNewWindow]
 *   Whether to run the task in a new window. If false, the current window will
 *   be used.
 * @param {boolean} [options.resetFlowManager]
 *   Whether to reset the internal state of TabsSetupFlowManager before running
 *   the task.
 * @param {Window} [options.win]
 *   The window in which to run the task.
 * @param {function(MozBrowser)} taskFn
 *   The task to run. It can be asynchronous.
 * @returns {any}
 *   The value returned by the task.
 */
async function withFirefoxView(
  { openNewWindow = false, resetFlowManager = true, win = null },
  taskFn
) {
  if (!win) {
    win = openNewWindow
      ? await BrowserTestUtils.openNewBrowserWindow()
      : Services.wm.getMostRecentBrowserWindow();
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
  if (openNewWindow) {
    await BrowserTestUtils.closeWindow(win);
  }
  return result;
}

function isFirefoxViewTabSelectedInWindow(win) {
  return win.gBrowser.selectedBrowser.currentURI.spec == getFirefoxViewURL();
}

export {
  init,
  withFirefoxView,
  assertFirefoxViewTab,
  assertFirefoxViewTabSelected,
  openFirefoxViewTab,
  closeFirefoxViewTab,
  isFirefoxViewTabSelectedInWindow,
  getFirefoxViewURL,
};
