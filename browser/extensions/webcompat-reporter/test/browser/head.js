const PREF_WC_REPORTER_ENABLED = "extensions.webcompat-reporter.enabled";
const PREF_WC_REPORTER_ENDPOINT = "extensions.webcompat-reporter.newIssueEndpoint";

const TEST_ROOT = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
const TEST_PAGE = TEST_ROOT + "test.html";
const NEW_ISSUE_PAGE = TEST_ROOT + "webcompat.html";

const WC_PAGE_ACTION_ID = "pageAction-panel-webcompat-reporter-button";
const WC_PAGE_ACTION_URLBAR_ID = "pageAction-urlbar-webcompat-reporter-button";

function isButtonDisabled() {
  return document.getElementById(WC_PAGE_ACTION_ID).disabled;
}

function isURLButtonEnabled() {
  return document.getElementById(WC_PAGE_ACTION_URLBAR_ID) !== null;
}

function openPageActions() {
  let dwu = window.windowUtils;
  return BrowserTestUtils.waitForCondition(() => {
    // Wait for the main page action button to become visible.  It's hidden for
    // some URIs, so depending on when this is called, it may not yet be quite
    // visible.  It's up to the caller to make sure it will be visible.
    info("Waiting for main page action button to have non-0 size");
    let bounds = dwu.getBoundsWithoutFlushing(BrowserPageActions.mainButtonNode);
    return bounds.width > 0 && bounds.height > 0;
  }).then(() => {
    // Wait for the panel to become open, by clicking the button if necessary.
    info("Waiting for main page action panel to be open");
    if (BrowserPageActions.panelNode.state == "open") {
      return Promise.resolve();
    }
    let shownPromise = promisePageActionPanelShown();
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    return shownPromise;
  }).then(() => {
    // Wait for items in the panel to become visible.
    return promisePageActionViewChildrenVisible(BrowserPageActions.mainViewNode);
  });
}

function promisePageActionPanelShown() {
  return new Promise(resolve => {
    if (BrowserPageActions.panelNode.state == "open") {
      executeSoon(resolve);
      return;
    }
    BrowserPageActions.panelNode.addEventListener("popupshown", () => {
      executeSoon(resolve);
    }, { once: true });
  });
}

function promisePageActionViewChildrenVisible(panelViewNode) {
  info("promisePageActionViewChildrenVisible waiting for a child node to be visible");
  let dwu = window.windowUtils;
  return BrowserTestUtils.waitForCondition(() => {
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

function pinToURLBar() {
  PageActions.actionForID("webcompat-reporter-button").pinnedToUrlbar = true;
}

function unpinFromURLBar() {
  PageActions.actionForID("webcompat-reporter-button").pinnedToUrlbar = false;
}
