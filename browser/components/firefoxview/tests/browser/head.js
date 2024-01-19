/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  getFirefoxViewURL,
  withFirefoxView,
  assertFirefoxViewTab,
  assertFirefoxViewTabSelected,
  openFirefoxViewTab,
  closeFirefoxViewTab,
  isFirefoxViewTabSelectedInWindow,
  init: FirefoxViewTestUtilsInit,
} = ChromeUtils.importESModule(
  "resource://testing-common/FirefoxViewTestUtils.sys.mjs"
);

/* exported testVisibility */

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { UIState } = ChromeUtils.importESModule(
  "resource://services-sync/UIState.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { FeatureCalloutMessages } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/FeatureCalloutMessages.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const triggeringPrincipal_base64 = E10SUtils.SERIALIZED_SYSTEMPRINCIPAL;
const { SessionStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SessionStoreTestUtils.sys.mjs"
);
SessionStoreTestUtils.init(this, window);
FirefoxViewTestUtilsInit(this, window);

ChromeUtils.defineESModuleGetters(this, {
  AboutWelcomeParent: "resource:///actors/AboutWelcomeParent.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  SyncedTabs: "resource://services-sync/SyncedTabs.sys.mjs",
  TabStateFlusher: "resource:///modules/sessionstore/TabStateFlusher.sys.mjs",
});

const calloutId = "feature-callout";
const calloutSelector = `#${calloutId}.featureCallout`;
const CTASelector = `#${calloutId} :is(.primary, .secondary)`;

/**
 * URLs used for browser_recently_closed_tabs_keyboard and
 * browser_firefoxview_accessibility
 */
const URLs = [
  "http://mochi.test:8888/browser/",
  "https://www.example.com/",
  "https://example.net/",
  "https://example.org/",
  "about:robots",
];

const syncedTabsData1 = [
  {
    id: 1,
    type: "client",
    name: "My desktop",
    clientType: "desktop",
    lastModified: 1655730486760,
    tabs: [
      {
        type: "tab",
        title: "Sandboxes - Sinon.JS",
        url: "https://sinonjs.org/releases/latest/sandbox/",
        icon: "https://sinonjs.org/assets/images/favicon.png",
        lastUsed: 1655391592, // Thu Jun 16 2022 14:59:52 GMT+0000
        client: 1,
      },
      {
        type: "tab",
        title: "Internet for people, not profits - Mozilla",
        url: "https://www.mozilla.org/",
        icon: "https://www.mozilla.org/media/img/favicons/mozilla/favicon.d25d81d39065.ico",
        lastUsed: 1655730486, // Mon Jun 20 2022 13:08:06 GMT+0000
        client: 1,
      },
    ],
  },
  {
    id: 2,
    type: "client",
    name: "My iphone",
    clientType: "phone",
    lastModified: 1655727832930,
    tabs: [
      {
        type: "tab",
        title: "The Guardian",
        url: "https://www.theguardian.com/",
        icon: "page-icon:https://www.theguardian.com/",
        lastUsed: 1655291890, // Wed Jun 15 2022 11:18:10 GMT+0000
        client: 2,
      },
      {
        type: "tab",
        title: "The Times",
        url: "https://www.thetimes.co.uk/",
        icon: "page-icon:https://www.thetimes.co.uk/",
        lastUsed: 1655727485, // Mon Jun 20 2022 12:18:05 GMT+0000
        client: 2,
      },
    ],
  },
];

async function clearAllParentTelemetryEvents() {
  // Clear everything.
  await TestUtils.waitForCondition(() => {
    Services.telemetry.clearEvents();
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).parent;
    return !events || !events.length;
  });
}

function testVisibility(browser, expected) {
  const { document } = browser.contentWindow;
  for (let [selector, shouldBeVisible] of Object.entries(
    expected.expectedVisible
  )) {
    const elem = document.querySelector(selector);
    if (shouldBeVisible) {
      ok(
        BrowserTestUtils.isVisible(elem),
        `Expected ${selector} to be visible`
      );
    } else {
      ok(BrowserTestUtils.isHidden(elem), `Expected ${selector} to be hidden`);
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
        ? BrowserTestUtils.isVisible(elem)
        : BrowserTestUtils.isHidden(elem);
    }
  );
}

async function waitForVisibleSetupStep(browser, expected) {
  const { document } = browser.contentWindow;

  const deck = document.querySelector(".sync-setup-container");
  const nextStepElem = deck.querySelector(expected.expectedVisible);
  const stepElems = deck.querySelectorAll(".setup-step");

  await BrowserTestUtils.waitForMutationCondition(
    deck,
    {
      attributeFilter: ["selected-view"],
    },
    () => {
      return BrowserTestUtils.isVisible(nextStepElem);
    }
  );

  for (let elem of stepElems) {
    if (elem == nextStepElem) {
      ok(
        BrowserTestUtils.isVisible(elem),
        `Expected ${elem.id || elem.className} to be visible`
      );
    } else {
      ok(
        BrowserTestUtils.isHidden(elem),
        `Expected ${elem.id || elem.className} to be hidden`
      );
    }
  }
}

var gMockFxaDevices = null;
var gUIStateStatus;
var gSandbox;
function setupSyncFxAMocks({ fxaDevices = null, state, syncEnabled = true }) {
  gUIStateStatus = state || UIState.STATUS_SIGNED_IN;
  if (gSandbox) {
    gSandbox.restore();
  }
  const sandbox = (gSandbox = sinon.createSandbox());
  gMockFxaDevices = fxaDevices;
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => fxaDevices);
  sandbox.stub(UIState, "get").callsFake(() => {
    return {
      status: gUIStateStatus,
      syncEnabled,
      email:
        gUIStateStatus === UIState.STATUS_NOT_CONFIGURED
          ? undefined
          : "email@example.com",
    };
  });

  return sandbox;
}

function setupRecentDeviceListMocks() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => [
    {
      id: 1,
      name: "My desktop",
      isCurrentDevice: true,
      type: "desktop",
    },
    {
      id: 2,
      name: "My iphone",
      type: "mobile",
    },
  ]);

  sandbox.stub(UIState, "get").returns({
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
    email: "email@example.com",
  });

  return sandbox;
}

function getMockTabData(clients) {
  return SyncedTabs._internal._createRecentTabsList(clients, 10);
}

async function setupListState(browser) {
  // Skip the synced tabs sign up flow to get to a loaded list state
  await SpecialPowers.pushPrefEnv({
    set: [["services.sync.engine.tabs", true]],
  });

  UIState.refresh();
  const recentFetchTime = Math.floor(Date.now() / 1000);
  info("updating lastFetch:" + recentFetchTime);
  Services.prefs.setIntPref("services.sync.lastTabFetch", recentFetchTime);

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  await waitForElementVisible(browser, "#tabpickup-steps", false);
  await waitForElementVisible(browser, "#tabpickup-tabs-container", true);

  const tabsContainer = browser.contentWindow.document.querySelector(
    "#tabpickup-tabs-container"
  );
  await tabsContainer.tabListAdded;
  await BrowserTestUtils.waitForMutationCondition(
    tabsContainer,
    { attributeFilter: ["class"], attributes: true },
    () => {
      return !tabsContainer.classList.contains("loading");
    }
  );
  info("tabsContainer isn't loading anymore, returning");
}

async function touchLastTabFetch() {
  // lastTabFetch stores a timestamp in *seconds*.
  const nowSeconds = Math.floor(Date.now() / 1000);
  info("updating lastFetch:" + nowSeconds);
  Services.prefs.setIntPref("services.sync.lastTabFetch", nowSeconds);
  // wait so all pref observers can complete
  await TestUtils.waitForTick();
}

let gUIStateSyncEnabled;
function setupMocks({ fxaDevices = null, state, syncEnabled = true }) {
  gUIStateStatus = state || UIState.STATUS_SIGNED_IN;
  gUIStateSyncEnabled = syncEnabled;
  if (gSandbox) {
    gSandbox.restore();
  }
  const sandbox = (gSandbox = sinon.createSandbox());
  gMockFxaDevices = fxaDevices;
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => fxaDevices);
  sandbox.stub(UIState, "get").callsFake(() => {
    return {
      status: gUIStateStatus,
      // Sometimes syncEnabled is not present on UIState, for example when the user signs
      // out the state is just { status: "not_configured" }
      ...(gUIStateSyncEnabled != undefined && {
        syncEnabled: gUIStateSyncEnabled,
      }),
    };
  });
  // This is converting the device list to a client list.
  // There are two primary differences:
  // 1. The client list doesn't return the current device.
  // 2. It uses clientType instead of type.
  let tabClients = fxaDevices ? [...fxaDevices] : [];
  for (let client of tabClients) {
    client.clientType = client.type;
  }
  tabClients = tabClients.filter(device => !device.isCurrentDevice);
  sandbox.stub(SyncedTabs, "getTabClients").callsFake(() => {
    return Promise.resolve(tabClients);
  });
  return sandbox;
}

async function tearDown(sandbox) {
  sandbox?.restore();
  Services.prefs.clearUserPref("services.sync.lastTabFetch");
}

const featureTourPref = "browser.firefox-view.feature-tour";
const launchFeatureTourIn = win => {
  const { FeatureCallout } = ChromeUtils.importESModule(
    "resource:///modules/FeatureCallout.sys.mjs"
  );
  let callout = new FeatureCallout({
    win,
    pref: { name: featureTourPref },
    location: "about:firefoxview",
    context: "content",
    theme: { preset: "themed-content" },
  });
  callout.showFeatureCallout();
  return callout;
};

/**
 * Returns a value that can be used to set
 * `browser.firefox-view.feature-tour` to change the feature tour's
 * UI state.
 *
 * @see FeatureCalloutMessages.sys.mjs for valid values of "screen"
 *
 * @param {number} screen The full ID of the feature callout screen
 * @returns {string} JSON string used to set
 * `browser.firefox-view.feature-tour`
 */
const getPrefValueByScreen = screen => {
  return JSON.stringify({
    screen: `FEATURE_CALLOUT_${screen}`,
    complete: false,
  });
};

/**
 * Wait for a feature callout screen of given parameters to be shown
 *
 * @param {Document} doc the document where the callout appears.
 * @param {string} screenPostfix The full ID of the feature callout screen.
 */
const waitForCalloutScreen = async (doc, screenPostfix) => {
  await BrowserTestUtils.waitForCondition(() =>
    doc.querySelector(`${calloutSelector}:not(.hidden) .${screenPostfix}`)
  );
};

/**
 * Waits for the feature callout screen to be removed.
 *
 * @param {Document} doc The document where the callout appears.
 */
const waitForCalloutRemoved = async doc => {
  await BrowserTestUtils.waitForCondition(() => {
    return !doc.body.querySelector(calloutSelector);
  });
};

/**
 * NOTE: Should be replaced with synthesizeMouseAtCenter for
 * simulating user input. See Bug 1798322
 *
 * Clicks the primary button in the feature callout dialog
 *
 * @param {document} doc Firefox View document
 */
const clickCTA = async doc => {
  doc.querySelector(CTASelector).click();
};

/**
 * Closes a feature callout via a click to the dismiss button.
 *
 * @param {Document} doc The document where the callout appears.
 */
const closeCallout = async doc => {
  // close the callout dialog
  const dismissBtn = doc.querySelector(`${calloutSelector} .dismiss-button`);
  if (!dismissBtn) {
    return;
  }
  doc.querySelector(`${calloutSelector} .dismiss-button`).click();
  await BrowserTestUtils.waitForCondition(() => {
    return !document.querySelector(calloutSelector);
  });
};

/**
 * Get a Feature Callout message by id.
 *
 * @param {string} id
 *   The message id.
 */
const getCalloutMessageById = id => {
  return {
    message: FeatureCalloutMessages.getMessages().find(m => m.id === id),
  };
};

/**
 * Create a sinon sandbox with `sendTriggerMessage` stubbed
 * to return a specified test message for featureCalloutCheck.
 *
 * @param {object} testMessage
 * @param {string} [source="about:firefoxview"]
 */
const createSandboxWithCalloutTriggerStub = (
  testMessage,
  source = "about:firefoxview"
) => {
  const firefoxViewMatch = sinon.match({
    id: "featureCalloutCheck",
    context: { source },
  });
  const sandbox = sinon.createSandbox();
  const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
  sendTriggerStub.withArgs(firefoxViewMatch).resolves(testMessage);
  sendTriggerStub.callThrough();
  return sandbox;
};

/**
 * A helper to check that correct telemetry was sent by AWSendEventTelemetry.
 * This is a wrapper around sinon's spy functionality.
 *
 * @example
 *  let spy = new TelemetrySpy();
 *  element.click();
 *  spy.assertCalledWith({ event: "CLICK" });
 *  spy.restore();
 */
class TelemetrySpy {
  /**
   * @param {object} [sandbox] A pre-existing sinon sandbox to build the spy in.
   *                           If not provided, a new sandbox will be created.
   */
  constructor(sandbox = sinon.createSandbox()) {
    this.sandbox = sandbox;
    this.spy = this.sandbox
      .spy(AboutWelcomeParent.prototype, "onContentMessage")
      .withArgs("AWPage:TELEMETRY_EVENT");
    registerCleanupFunction(() => this.restore());
  }
  /**
   * Assert that AWSendEventTelemetry sent the expected telemetry object.
   *
   * @param {object} expectedData
   */
  assertCalledWith(expectedData) {
    let match = this.spy.calledWith("AWPage:TELEMETRY_EVENT", expectedData);
    if (match) {
      ok(true, "Expected telemetry sent");
    } else if (this.spy.called) {
      ok(
        false,
        "Wrong telemetry sent: " + JSON.stringify(this.spy.lastCall.args)
      );
    } else {
      ok(false, "No telemetry sent");
    }
  }
  reset() {
    this.spy.resetHistory();
  }
  restore() {
    this.sandbox.restore();
  }
}

/**
 * Helper function to open and close a tab so the recently
 * closed tabs list can have data.
 *
 * @param {string} url
 * @returns {Promise} Promise that resolves when the session store
 * has been updated after closing the tab.
 */
async function open_then_close(url, win = window) {
  return SessionStoreTestUtils.openAndCloseTab(win, url);
}

/**
 * Clears session history. Used to clear out the recently closed tabs list.
 *
 */
function clearHistory() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
}

/**
 * Cleanup function for tab pickup tests.
 *
 */
function cleanup_tab_pickup() {
  Services.prefs.clearUserPref("services.sync.engine.tabs");
  Services.prefs.clearUserPref("services.sync.lastTabFetch");
}

function isFirefoxViewTabSelected(win = window) {
  return isFirefoxViewTabSelectedInWindow(win);
}

function promiseAllButPrimaryWindowClosed() {
  let windows = [];
  for (let win of BrowserWindowTracker.orderedWindows) {
    if (win != window) {
      windows.push(win);
    }
  }
  return Promise.all(windows.map(BrowserTestUtils.closeWindow));
}

registerCleanupFunction(() => {
  // ensure all the stubs are restored, regardless of any exceptions
  // that might have prevented it
  gSandbox?.restore();
});

function navigateToCategory(document, category) {
  const navigation = document.querySelector("fxview-category-navigation");
  let navButton = Array.from(navigation.categoryButtons).filter(
    categoryButton => {
      return categoryButton.name === category;
    }
  )[0];
  navButton.buttonEl.click();
}

async function navigateToCategoryAndWait(document, category) {
  info(`navigateToCategoryAndWait, for ${category}`);
  const navigation = document.querySelector("fxview-category-navigation");
  const win = document.ownerGlobal;
  SimpleTest.promiseFocus(win);
  let navButton = Array.from(navigation.categoryButtons).find(
    categoryButton => {
      return categoryButton.name === category;
    }
  );
  const namedDeck = document.querySelector("named-deck");

  await BrowserTestUtils.waitForCondition(
    () => navButton.getBoundingClientRect().height,
    `Waiting for ${category} button to be clickable`
  );

  EventUtils.synthesizeMouseAtCenter(navButton, {}, win);

  await BrowserTestUtils.waitForCondition(() => {
    let selectedView = Array.from(namedDeck.children).find(
      child => child.slot == "selected"
    );
    return (
      namedDeck.selectedViewName == category &&
      selectedView?.getBoundingClientRect().height
    );
  }, `Waiting for ${category} to be visible`);
}

/**
 * Switch to the Firefox View tab.
 *
 * @param {Window} [win]
 *   The window to use, if specified. Defaults to the global window instance.
 * @returns {Promise<MozTabbrowserTab>}
 *   The tab switched to.
 */
async function switchToFxViewTab(win = window) {
  return BrowserTestUtils.switchTab(win.gBrowser, win.FirefoxViewHandler.tab);
}

function isElInViewport(element) {
  const boundingRect = element.getBoundingClientRect();
  return (
    boundingRect.top >= 0 &&
    boundingRect.left >= 0 &&
    boundingRect.bottom <=
      (window.innerHeight || document.documentElement.clientHeight) &&
    boundingRect.right <=
      (window.innerWidth || document.documentElement.clientWidth)
  );
}

// TODO once we port over old tests, helpers and cleanup old firefox view
// we should decide whether to keep this or openFirefoxViewTab.
async function clickFirefoxViewButton(win) {
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#firefox-view-button",
    { type: "mousedown" },
    win.browsingContext
  );
}

/**
 * Wait for and assert telemetry events.
 *
 * @param {Array} eventDetails
 *   Nested array of event details
 */
async function telemetryEvent(eventDetails) {
  await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;
      return events && events.length >= 1;
    },
    "Waiting for firefoxview_next telemetry event.",
    200,
    100
  );

  TelemetryTestUtils.assertEvents(
    eventDetails,
    { category: "firefoxview_next" },
    { clear: true, process: "parent" }
  );
}
