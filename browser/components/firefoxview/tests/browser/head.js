/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable no-unused-vars */
function testVisibility(browser, expected) {
  const { document } = browser.contentWindow;
  for (let [selector, shouldBeVisible] of Object.entries(
    expected.expectedVisible
  )) {
    const elem = document.querySelector(selector);
    if (shouldBeVisible) {
      ok(
        BrowserTestUtils.is_visible(elem),
        `Expected ${selector} to be visible`
      );
    } else {
      ok(BrowserTestUtils.is_hidden(elem), `Expected ${selector} to be hidden`);
    }
  }
}

async function waitForElementVisible(browser, selector, isVisible = true) {
  const { document } = browser.contentWindow;
  const elem = document.querySelector(selector);
  if (!isVisible && !elem) {
    return;
  }
  ok(elem, `Got element with selector: ${selector}`);

  await BrowserTestUtils.waitForMutationCondition(
    elem,
    {
      attributeFilter: ["hidden"],
    },
    () => {
      return isVisible
        ? BrowserTestUtils.is_visible(elem)
        : BrowserTestUtils.is_hidden(elem);
    }
  );
}

function assertFirefoxViewTab(w = window) {
  ok(w.FirefoxViewHandler.tab, "Firefox View tab exists");
  ok(w.FirefoxViewHandler.tab?.hidden, "Firefox View tab is hidden");
  is(
    w.gBrowser.tabs.indexOf(w.FirefoxViewHandler.tab),
    0,
    "Firefox View tab is the first tab"
  );
  is(
    w.gBrowser.visibleTabs.indexOf(w.FirefoxViewHandler.tab),
    -1,
    "Firefox View tab is not in the list of visible tabs"
  );
}

async function openFirefoxViewTab(w = window) {
  ok(
    !w.FirefoxViewHandler.tab,
    "Firefox View tab doesn't exist prior to clicking the button"
  );
  info("Clicking the Firefox View button");
  await EventUtils.synthesizeMouseAtCenter(
    w.document.getElementById("firefox-view-button"),
    {},
    w
  );
  assertFirefoxViewTab(w);
  is(w.gBrowser.tabContainer.selectedIndex, 0, "Firefox View tab is selected");
  await BrowserTestUtils.browserLoaded(w.FirefoxViewHandler.tab.linkedBrowser);
  return w.FirefoxViewHandler.tab;
}

function closeFirefoxViewTab(w = window) {
  w.gBrowser.removeTab(w.FirefoxViewHandler.tab);
  ok(
    !w.FirefoxViewHandler.tab,
    "Reference to Firefox View tab got removed when closing the tab"
  );
}

async function withFirefoxView(
  { resetFlowManager = true, win = window },
  taskFn
) {
  if (resetFlowManager) {
    const { TabsSetupFlowManager } = ChromeUtils.importESModule(
      "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
    );
    // reset internal state so we aren't reacting to whatever state the last invocation left behind
    TabsSetupFlowManager.resetInternalState();
  }
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
  return Promise.resolve(result);
}
