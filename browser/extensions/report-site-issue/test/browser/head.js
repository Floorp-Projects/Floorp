"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

const { Management } = ChromeUtils.import(
  "resource://gre/modules/Extension.jsm"
);

const PREF_WC_REPORTER_ENABLED = "extensions.webcompat-reporter.enabled";
const PREF_WC_REPORTER_ENDPOINT =
  "extensions.webcompat-reporter.newIssueEndpoint";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const TEST_PAGE = TEST_ROOT + "test.html";
const FRAMEWORKS_TEST_PAGE = TEST_ROOT + "frameworks.html";
const FASTCLICK_TEST_PAGE = TEST_ROOT + "fastclick.html";
const NEW_ISSUE_PAGE = TEST_ROOT + "webcompat.html";

const WC_ADDON_ID = "webcompat-reporter@mozilla.org";

const WC_PAGE_ACTION_ID = "webcompat-reporter_mozilla_org";
const WC_PAGE_ACTION_PANEL_ID =
  "pageAction-panel-webcompat-reporter_mozilla_org";
const WC_PAGE_ACTION_URLBAR_ID =
  "pageAction-urlbar-webcompat-reporter_mozilla_org";

const oldPAadd = PageActions.addAction;
const placedSignal = "webext-page-action-placed";
PageActions.addAction = function(action) {
  oldPAadd.call(this, action);
  if (action.id === WC_PAGE_ACTION_ID) {
    Services.obs.notifyObservers(null, placedSignal);
  }
  return action;
};
const oldPAremoved = PageActions.onActionRemoved;
const removedSignal = "webext-page-action-removed";
PageActions.onActionRemoved = function(action) {
  oldPAremoved.call(this, action);
  if (action.id === WC_PAGE_ACTION_ID) {
    Services.obs.notifyObservers(null, removedSignal);
  }
  return action;
};

function promisePageActionSignal(signal) {
  return new Promise(done => {
    const obs = function() {
      Services.obs.removeObserver(obs, signal);
      done();
    };
    Services.obs.addObserver(obs, signal);
  });
}

function promisePageActionPlaced() {
  return promisePageActionSignal(placedSignal);
}

function promisePageActionRemoved() {
  return promisePageActionSignal(removedSignal);
}

async function promiseAddonEnabled() {
  const addon = await AddonManager.getAddonByID(WC_ADDON_ID);
  if (addon.isActive) {
    return;
  }
  const pref = SpecialPowers.Services.prefs.getBoolPref(
    PREF_WC_REPORTER_ENABLED,
    false
  );
  if (!pref) {
    SpecialPowers.Services.prefs.setBoolPref(PREF_WC_REPORTER_ENABLED, true);
  }
  await promisePageActionPlaced();
}

async function isPanelItemEnabled() {
  const icon = document.getElementById(WC_PAGE_ACTION_PANEL_ID);
  return icon && !icon.disabled;
}

async function isPanelItemDisabled() {
  const icon = document.getElementById(WC_PAGE_ACTION_PANEL_ID);
  return icon && icon.disabled;
}

async function isPanelItemPresent() {
  return document.getElementById(WC_PAGE_ACTION_PANEL_ID) !== null;
}

async function isURLButtonPresent() {
  return document.getElementById(WC_PAGE_ACTION_URLBAR_ID) !== null;
}

function openPageActions() {
  let dwu = window.windowUtils;
  return BrowserTestUtils.waitForCondition(() => {
    // Wait for the main page action button to become visible.  It's hidden for
    // some URIs, so depending on when this is called, it may not yet be quite
    // visible.  It's up to the caller to make sure it will be visible.
    info("Waiting for main page action button to have non-0 size");
    let bounds = dwu.getBoundsWithoutFlushing(
      BrowserPageActions.mainButtonNode
    );
    return bounds.width > 0 && bounds.height > 0;
  })
    .then(() => {
      // Wait for the panel to become open, by clicking the button if necessary.
      info("Waiting for main page action panel to be open");
      if (BrowserPageActions.panelNode.state == "open") {
        return Promise.resolve();
      }
      let shownPromise = promisePageActionPanelShown();
      EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
      return shownPromise;
    })
    .then(() => {
      // Wait for items in the panel to become visible.
      return promisePageActionViewChildrenVisible(
        BrowserPageActions.mainViewNode
      );
    });
}

function promisePageActionPanelShown() {
  return new Promise(resolve => {
    if (BrowserPageActions.panelNode.state == "open") {
      executeSoon(resolve);
      return;
    }
    BrowserPageActions.panelNode.addEventListener(
      "popupshown",
      () => {
        executeSoon(resolve);
      },
      { once: true }
    );
  });
}

function promisePageActionViewChildrenVisible(panelViewNode) {
  info(
    "promisePageActionViewChildrenVisible waiting for a child node to be visible"
  );
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
  PageActions.actionForID(WC_PAGE_ACTION_ID).pinnedToUrlbar = true;
}

function unpinFromURLBar() {
  PageActions.actionForID(WC_PAGE_ACTION_ID).pinnedToUrlbar = false;
}

async function startIssueServer() {
  const landingTemplate = await new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open("GET", NEW_ISSUE_PAGE);
    xhr.onload = () => {
      resolve(xhr.responseText);
    };
    xhr.onerror = reject;
    xhr.send();
  });

  const { HttpServer } = ChromeUtils.import(
    "resource://testing-common/httpd.js"
  );
  const server = new HttpServer();

  registerCleanupFunction(async function cleanup() {
    await new Promise(resolve => server.stop(resolve));
  });

  server.registerPathHandler("/new", function(request, response) {
    response.setHeader("Content-Type", "text/html", false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(landingTemplate);
  });

  server.start(-1);
  return `http://localhost:${server.identity.primaryPort}/new`;
}
