/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyModuleGetters(this, {
  EnterprisePolicyTesting:
    "resource://testing-common/EnterprisePolicyTesting.jsm",
  ExtensionCommon: "resource://gre/modules/ExtensionCommon.jsm",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
});

async function promisePageActionPanelOpen(win = window, eventDict = {}) {
  await BrowserTestUtils.waitForCondition(() => {
    // Wait for the main page action button to become visible.  It's hidden for
    // some URIs, so depending on when this is called, it may not yet be quite
    // visible.  It's up to the caller to make sure it will be visible.
    info("Waiting for main page action button to have non-0 size");
    let bounds = win.windowUtils.getBoundsWithoutFlushing(
      win.BrowserPageActions.mainButtonNode
    );
    return bounds.width > 0 && bounds.height > 0;
  });

  // Wait for the panel to become open, by clicking the button if necessary.
  info("Waiting for main page action panel to be open");
  if (win.BrowserPageActions.panelNode.state == "open") {
    return;
  }
  let shownPromise = promisePageActionPanelShown(win);
  EventUtils.synthesizeMouseAtCenter(
    win.BrowserPageActions.mainButtonNode,
    eventDict,
    win
  );
  await shownPromise;
  info("Wait for items in the panel to become visible.");
  await promisePageActionViewChildrenVisible(
    win.BrowserPageActions.mainViewNode,
    win
  );
}

function promisePageActionPanelShown(win = window) {
  return promisePanelShown(win.BrowserPageActions.panelNode, win);
}

function promisePageActionPanelHidden(win = window) {
  return promisePanelHidden(win.BrowserPageActions.panelNode, win);
}

function promisePanelShown(panelIDOrNode, win = window) {
  return promisePanelEvent(panelIDOrNode, "popupshown", win);
}

function promisePanelHidden(panelIDOrNode, win = window) {
  return promisePanelEvent(panelIDOrNode, "popuphidden", win);
}

function promisePanelEvent(panelIDOrNode, eventType, win = window) {
  return new Promise(resolve => {
    let panel = panelIDOrNode;
    if (typeof panel == "string") {
      panel = win.document.getElementById(panelIDOrNode);
      if (!panel) {
        throw new Error(`Panel with ID "${panelIDOrNode}" does not exist.`);
      }
    }
    if (
      (eventType == "popupshown" && panel.state == "open") ||
      (eventType == "popuphidden" && panel.state == "closed")
    ) {
      executeSoon(() => resolve(panel));
      return;
    }
    panel.addEventListener(
      eventType,
      () => {
        executeSoon(() => resolve(panel));
      },
      { once: true }
    );
  });
}

async function promisePageActionViewChildrenVisible(
  panelViewNode,
  win = window
) {
  info(
    "promisePageActionViewChildrenVisible waiting for a child node to be visible"
  );
  await new Promise(win.requestAnimationFrame);
  let dwu = win.windowUtils;
  return TestUtils.waitForCondition(() => {
    let bodyNode = panelViewNode.firstElementChild;
    for (let childNode of bodyNode.children) {
      let bounds = dwu.getBoundsWithoutFlushing(childNode);
      if (bounds.width > 0 && bounds.height > 0) {
        return true;
      }
    }
    return false;
  });
}

function promiseAddonUninstalled(addonId) {
  return new Promise(resolve => {
    let listener = {};
    listener.onUninstalled = addon => {
      if (addon.id == addonId) {
        AddonManager.removeAddonListener(listener);
        resolve();
      }
    };
    AddonManager.addAddonListener(listener);
  });
}

async function promiseAnimationFrame(win = window) {
  await new Promise(resolve => win.requestAnimationFrame(resolve));
  await win.promiseDocumentFlushed(() => {});
}

async function promisePopupNotShown(id, win = window) {
  let deferred = PromiseUtils.defer();
  function listener(e) {
    deferred.reject("Unexpected popupshown");
  }
  let panel = win.document.getElementById(id);
  panel.addEventListener("popupshown", listener);
  try {
    await Promise.race([
      deferred.promise,
      new Promise(resolve => {
        /* eslint-disable mozilla/no-arbitrary-setTimeout */
        win.setTimeout(resolve, 300);
      }),
    ]);
  } finally {
    panel.removeEventListener("popupshown", listener);
  }
}

// TODO (Bug 1700780): Why is this necessary? Without this trick the test
// fails intermittently on Ubuntu.
function promiseStableResize(expectedWidth, win = window) {
  let deferred = PromiseUtils.defer();
  let id;
  function listener() {
    win.clearTimeout(id);
    info(`Got resize event: ${win.innerWidth} x ${win.innerHeight}`);
    if (win.innerWidth <= expectedWidth) {
      id = win.setTimeout(() => {
        win.removeEventListener("resize", listener);
        deferred.resolve();
      }, 100);
    }
  }
  win.addEventListener("resize", listener);
  win.resizeTo(expectedWidth, win.outerHeight);
  return deferred.promise;
}
