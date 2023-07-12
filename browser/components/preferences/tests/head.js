/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const kDefaultWait = 2000;

function is_element_visible(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(!BrowserTestUtils.is_hidden(aElement), aMsg);
}

function is_element_hidden(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_hidden(aElement), aMsg);
}

function open_preferences(aCallback) {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:preferences");
  let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  newTabBrowser.addEventListener(
    "Initialized",
    function () {
      aCallback(gBrowser.contentWindow);
    },
    { capture: true, once: true }
  );
}

function openAndLoadSubDialog(
  aURL,
  aFeatures = null,
  aParams = null,
  aClosingCallback = null
) {
  let promise = promiseLoadSubDialog(aURL);
  content.gSubDialog.open(
    aURL,
    { features: aFeatures, closingCallback: aClosingCallback },
    aParams
  );
  return promise;
}

function promiseLoadSubDialog(aURL) {
  return new Promise((resolve, reject) => {
    content.gSubDialog._dialogStack.addEventListener(
      "dialogopen",
      function dialogopen(aEvent) {
        if (
          aEvent.detail.dialog._frame.contentWindow.location == "about:blank"
        ) {
          return;
        }
        content.gSubDialog._dialogStack.removeEventListener(
          "dialogopen",
          dialogopen
        );

        is(
          aEvent.detail.dialog._frame.contentWindow.location.toString(),
          aURL,
          "Check the proper URL is loaded"
        );

        // Check visibility
        is_element_visible(aEvent.detail.dialog._overlay, "Overlay is visible");

        // Check that stylesheets were injected
        let expectedStyleSheetURLs =
          aEvent.detail.dialog._injectedStyleSheets.slice(0);
        for (let styleSheet of aEvent.detail.dialog._frame.contentDocument
          .styleSheets) {
          let i = expectedStyleSheetURLs.indexOf(styleSheet.href);
          if (i >= 0) {
            info("found " + styleSheet.href);
            expectedStyleSheetURLs.splice(i, 1);
          }
        }
        is(
          expectedStyleSheetURLs.length,
          0,
          "All expectedStyleSheetURLs should have been found"
        );

        // Wait for the next event tick to make sure the remaining part of the
        // testcase runs after the dialog gets ready for input.
        executeSoon(() => resolve(aEvent.detail.dialog._frame.contentWindow));
      }
    );
  });
}

async function openPreferencesViaOpenPreferencesAPI(aPane, aOptions) {
  let finalPaneEvent = Services.prefs.getBoolPref("identity.fxaccounts.enabled")
    ? "sync-pane-loaded"
    : "privacy-pane-loaded";
  let finalPrefPaneLoaded = TestUtils.topicObserved(finalPaneEvent, () => true);
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  openPreferences(aPane, aOptions);
  let newTabBrowser = gBrowser.selectedBrowser;

  if (!newTabBrowser.contentWindow) {
    await BrowserTestUtils.waitForEvent(newTabBrowser, "Initialized", true);
    await BrowserTestUtils.waitForEvent(newTabBrowser.contentWindow, "load");
    await finalPrefPaneLoaded;
  }

  let win = gBrowser.contentWindow;
  let selectedPane = win.history.state;
  if (!aOptions || !aOptions.leaveOpen) {
    gBrowser.removeCurrentTab();
  }
  return { selectedPane };
}

async function runSearchInput(input) {
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");
  searchInput.focus();
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow,
    "PreferencesSearchCompleted",
    evt => evt.detail == input
  );
  EventUtils.sendString(input);
  await searchCompletedPromise;
}

async function evaluateSearchResults(
  keyword,
  searchResults,
  includeExperiments = false
) {
  searchResults = Array.isArray(searchResults)
    ? searchResults
    : [searchResults];
  searchResults.push("header-searchResults");

  await runSearchInput(keyword);

  let mainPrefTag = gBrowser.contentDocument.getElementById("mainPrefPane");
  for (let i = 0; i < mainPrefTag.childElementCount; i++) {
    let child = mainPrefTag.children[i];
    if (!includeExperiments && child.id?.startsWith("pane-experimental")) {
      continue;
    }
    if (searchResults.includes(child.id)) {
      is_element_visible(child, `${child.id} should be in search results`);
    } else if (child.id) {
      is_element_hidden(child, `${child.id} should not be in search results`);
    }
  }
}

function waitForMutation(target, opts, cb) {
  return new Promise(resolve => {
    let observer = new MutationObserver(() => {
      if (!cb || cb(target)) {
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(target, opts);
  });
}

// Used to add sample experimental features for testing. To use, create
// a DefinitionServer, then call addDefinition as needed.
class DefinitionServer {
  constructor(definitionOverrides = []) {
    let { HttpServer } = ChromeUtils.importESModule(
      "resource://testing-common/httpd.sys.mjs"
    );

    this.server = new HttpServer();
    this.server.registerPathHandler("/definitions.json", this);
    this.definitions = {};

    for (const override of definitionOverrides) {
      this.addDefinition(override);
    }

    this.server.start();
    registerCleanupFunction(
      () => new Promise(resolve => this.server.stop(resolve))
    );
  }

  // for nsIHttpRequestHandler
  handle(request, response) {
    response.write(JSON.stringify(this.definitions));
  }

  get definitionsUrl() {
    const { primaryScheme, primaryHost, primaryPort } = this.server.identity;
    return `${primaryScheme}://${primaryHost}:${primaryPort}/definitions.json`;
  }

  addDefinition(overrides = {}) {
    const definition = {
      id: "test-feature",
      // These l10n IDs are just random so we have some text to display
      title: "experimental-features-media-jxl",
      description: "pane-experimental-description2",
      restartRequired: false,
      type: "boolean",
      preference: "test.feature",
      defaultValue: false,
      isPublic: false,
      ...overrides,
    };
    // convert targeted values, used by fromId
    definition.isPublic = { default: definition.isPublic };
    definition.defaultValue = { default: definition.defaultValue };
    this.definitions[definition.id] = definition;
    return definition;
  }
}

/**
 * Creates observer that waits for and then compares all perm-changes with the observances in order.
 * @param {Array} observances permission changes to observe (order is important)
 * @returns {Promise} Promise object that resolves once all permission changes have been observed
 */
function createObserveAllPromise(observances) {
  // Create new promise that resolves once all items
  // in observances array have been observed.
  return new Promise(resolve => {
    let permObserver = {
      observe(aSubject, aTopic, aData) {
        if (aTopic != "perm-changed") {
          return;
        }

        if (!observances.length) {
          // See bug 1063410
          return;
        }

        let permission = aSubject.QueryInterface(Ci.nsIPermission);
        let expected = observances.shift();

        info(
          `observed perm-changed for ${permission.principal.origin} (remaining ${observances.length})`
        );

        is(aData, expected.data, "type of message should be the same");
        for (let prop of ["type", "capability", "expireType"]) {
          if (expected[prop]) {
            is(
              permission[prop],
              expected[prop],
              `property: "${prop}" should be equal (${permission.principal.origin})`
            );
          }
        }

        if (expected.origin) {
          is(
            permission.principal.origin,
            expected.origin,
            `property: "origin" should be equal (${permission.principal.origin})`
          );
        }

        if (!observances.length) {
          Services.obs.removeObserver(permObserver, "perm-changed");
          executeSoon(resolve);
        }
      },
    };
    Services.obs.addObserver(permObserver, "perm-changed");
  });
}

/**
 * Waits for preference to be set and asserts the value.
 * @param {string} pref - Preference key.
 * @param {*} expectedValue - Expected value of the preference.
 * @param {string} message - Assertion message.
 */
async function waitForAndAssertPrefState(pref, expectedValue, message) {
  await TestUtils.waitForPrefChange(pref, value => {
    if (value != expectedValue) {
      return false;
    }
    is(value, expectedValue, message);
    return true;
  });
}

/**
 * The Relay promo is not shown for distributions with a custom FxA instance,
 * since Relay requires an account on our own server. These prefs are set to a
 * dummy address by the test harness, filling the prefs with a "user value."
 * This temporarily sets the default value equal to the dummy value, so that
 * Firefox thinks we've configured the correct FxA server.
 * @returns {Promise<MockFxAUtilityFunctions>} { mock, unmock }
 */
async function mockDefaultFxAInstance() {
  /**
   * @typedef {Object} MockFxAUtilityFunctions
   * @property {function():void} mock - Makes the dummy values default, creating
   *                             the illusion of a production FxA instance.
   * @property {function():void} unmock - Restores the true defaults, creating
   *                             the illusion of a custom FxA instance.
   */

  const defaultPrefs = Services.prefs.getDefaultBranch("");
  const userPrefs = Services.prefs.getBranch("");
  const realAuth = defaultPrefs.getCharPref("identity.fxaccounts.auth.uri");
  const realRoot = defaultPrefs.getCharPref("identity.fxaccounts.remote.root");
  const mockAuth = userPrefs.getCharPref("identity.fxaccounts.auth.uri");
  const mockRoot = userPrefs.getCharPref("identity.fxaccounts.remote.root");
  const mock = () => {
    defaultPrefs.setCharPref("identity.fxaccounts.auth.uri", mockAuth);
    defaultPrefs.setCharPref("identity.fxaccounts.remote.root", mockRoot);
    userPrefs.clearUserPref("identity.fxaccounts.auth.uri");
    userPrefs.clearUserPref("identity.fxaccounts.remote.root");
  };
  const unmock = () => {
    defaultPrefs.setCharPref("identity.fxaccounts.auth.uri", realAuth);
    defaultPrefs.setCharPref("identity.fxaccounts.remote.root", realRoot);
    userPrefs.setCharPref("identity.fxaccounts.auth.uri", mockAuth);
    userPrefs.setCharPref("identity.fxaccounts.remote.root", mockRoot);
  };

  mock();
  registerCleanupFunction(unmock);

  return { mock, unmock };
}
