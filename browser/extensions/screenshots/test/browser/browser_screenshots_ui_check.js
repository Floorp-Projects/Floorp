"use strict";

ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");

const BUTTON_ID = "pageAction-panel-screenshots_mozilla_org";

function checkElements(expectPresent, l) {
  for (const id of l) {
    is(!!document.getElementById(id), expectPresent, "element " + id + (expectPresent ? " is" : " is not") + " present");
  }
}

async function togglePageActionPanel() {
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelEvent("popuphidden");
}

function promiseOpenPageActionPanel() {
  return BrowserTestUtils.waitForCondition(() => {
    // Wait for the main page action button to become visible.  It's hidden for
    // some URIs, so depending on when this is called, it may not yet be quite
    // visible.  It's up to the caller to make sure it will be visible.
    info("Waiting for main page action button to have non-0 size");
    const bounds = window.windowUtils.getBoundsWithoutFlushing(BrowserPageActions.mainButtonNode);
    return bounds.width > 0 && bounds.height > 0;
  }).then(() => {
    // Wait for the panel to become open, by clicking the button if necessary.
    info("Waiting for main page action panel to be open");
    if (BrowserPageActions.panelNode.state === "open") {
      return Promise.resolve();
    }
    const shownPromise = promisePageActionPanelEvent("popupshown");
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    return shownPromise;
  }).then(() => {
    // Wait for items in the panel to become visible.
    return promisePageActionViewChildrenVisible(BrowserPageActions.mainViewNode);
  });
}

function promisePageActionPanelEvent(eventType) {
  return new Promise(resolve => {
    const panel = BrowserPageActions.panelNode;
    if ((eventType === "popupshown" && panel.state === "open") ||
        (eventType === "popuphidden" && panel.state === "closed")) {
      executeSoon(resolve);
      return;
    }
    panel.addEventListener(eventType, () => {
      executeSoon(resolve);
    }, { once: true });
  });
}

function promisePageActionViewChildrenVisible(panelViewNode) {
  info("promisePageActionViewChildrenVisible waiting for a child node to be visible");
  return BrowserTestUtils.waitForCondition(() => {
    const bodyNode = panelViewNode.firstElementChild;
    for (const childNode of bodyNode.children) {
      const bounds = window.windowUtils.getBoundsWithoutFlushing(childNode);
      if (bounds.width > 0 && bounds.height > 0) {
        return true;
      }
    }
    return false;
  });
}

add_task(async function() {
  // If Screenshots was disabled, enable it just for this test.
  const addon = await AddonManager.getAddonByID("screenshots@mozilla.org");
  const isEnabled = addon.enabled;
  if (!isEnabled) {
    await addon.enable({allowSystemAddons: true});
    registerCleanupFunction(async () => {
      await addon.disable({allowSystemAddons: true});
    });
  }

  // Toggle the page action panel to get it to rebuild itself.  An actionable
  // page must be opened first.
  const url = "http://example.com/browser_screenshots_ui_check";
  await BrowserTestUtils.withNewTab(url, async () => { // eslint-disable-line space-before-function-paren
    await togglePageActionPanel();

    await BrowserTestUtils.waitForCondition(
      () => document.getElementById(BUTTON_ID),
      "Screenshots button should be present", 100, 100);

    checkElements(true, [BUTTON_ID]);
  });
});
