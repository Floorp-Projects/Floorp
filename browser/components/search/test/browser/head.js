/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  FormHistoryTestUtils:
    "resource://testing-common/FormHistoryTestUtils.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  ADLINK_CHECK_TIMEOUT_MS: "resource:///actors/SearchSERPTelemetryChild.jsm",
  AddonTestUtils: "resource://testing-common/AddonTestUtils.jsm",
  CustomizableUITestUtils:
    "resource://testing-common/CustomizableUITestUtils.jsm",
});

let gCUITestUtils = new CustomizableUITestUtils(window);

AddonTestUtils.initMochitest(this);
SearchTestUtils.init(this);

/**
 * Recursively compare two objects and check that every property of expectedObj has the same value
 * on actualObj.
 *
 * @param {object} expectedObj
 *        The expected object to find.
 * @param {object} actualObj
 *        The object to inspect.
 * @param {string} name
 *        The name of the engine, used for test detail logging.
 */
function isSubObjectOf(expectedObj, actualObj, name) {
  for (let prop in expectedObj) {
    if (typeof expectedObj[prop] == "function") {
      continue;
    }
    if (expectedObj[prop] instanceof Object) {
      is(
        actualObj[prop].length,
        expectedObj[prop].length,
        name + "[" + prop + "]"
      );
      isSubObjectOf(
        expectedObj[prop],
        actualObj[prop],
        name + "[" + prop + "]"
      );
    } else {
      is(actualObj[prop], expectedObj[prop], name + "[" + prop + "]");
    }
  }
}

function getLocale() {
  return Services.locale.requestedLocale || undefined;
}

function promiseEvent(aTarget, aEventName, aPreventDefault) {
  function cancelEvent(event) {
    if (aPreventDefault) {
      event.preventDefault();
    }

    return true;
  }

  return BrowserTestUtils.waitForEvent(aTarget, aEventName, false, cancelEvent);
}

// Get an array of the one-off buttons.
function getOneOffs() {
  let oneOffs = [];
  let searchPopup = document.getElementById("PopupSearchAutoComplete");
  let oneOffsContainer = searchPopup.searchOneOffsContainer;
  let oneOff = oneOffsContainer.querySelector(".search-panel-one-offs");
  for (oneOff = oneOff.firstChild; oneOff; oneOff = oneOff.nextSibling) {
    if (oneOff.nodeType == Node.ELEMENT_NODE) {
      oneOffs.push(oneOff);
    }
  }
  return oneOffs;
}

async function typeInSearchField(browser, text, fieldName) {
  await SpecialPowers.spawn(browser, [[fieldName, text]], async function([
    contentFieldName,
    contentText,
  ]) {
    // Put the focus on the search box.
    let searchInput = content.document.getElementById(contentFieldName);
    searchInput.focus();
    searchInput.value = contentText;
  });
}

XPCOMUtils.defineLazyGetter(this, "searchCounts", () => {
  return Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
});

XPCOMUtils.defineLazyGetter(this, "SEARCH_AD_CLICK_SCALARS", () => {
  const sources = [
    ...BrowserSearchTelemetry.KNOWN_SEARCH_SOURCES.values(),
    "unknown",
  ];
  return [
    ...sources.map(v => `browser.search.withads.${v}`),
    ...sources.map(v => `browser.search.adclicks.${v}`),
  ];
});

// Ad links are processed after a small delay. We need to allow tests to wait
// for that before checking telemetry, otherwise the received values may be
// too small in some cases.
function promiseWaitForAdLinkCheck() {
  return new Promise(resolve =>
    /* eslint-disable-next-line mozilla/no-arbitrary-setTimeout */
    setTimeout(resolve, ADLINK_CHECK_TIMEOUT_MS)
  );
}

async function assertSearchSourcesTelemetry(
  expectedHistograms,
  expectedScalars
) {
  let histSnapshot = {};
  let scalars = {};

  // This used to rely on the implied 100ms initial timer of
  // TestUtils.waitForCondition. See bug 1515466.
  await new Promise(resolve => setTimeout(resolve, 100));

  await TestUtils.waitForCondition(() => {
    histSnapshot = searchCounts.snapshot();
    return (
      Object.getOwnPropertyNames(histSnapshot).length ==
      Object.getOwnPropertyNames(expectedHistograms).length
    );
  }, "should have the correct number of histograms");

  if (Object.entries(expectedScalars).length) {
    await TestUtils.waitForCondition(() => {
      scalars =
        Services.telemetry.getSnapshotForKeyedScalars("main", false).parent ||
        {};
      return Object.getOwnPropertyNames(expectedScalars).every(
        scalar => scalar in scalars
      );
    }, "should have the expected keyed scalars");
  }

  Assert.equal(
    Object.getOwnPropertyNames(histSnapshot).length,
    Object.getOwnPropertyNames(expectedHistograms).length,
    "Should only have one key"
  );

  for (let [key, value] of Object.entries(expectedHistograms)) {
    Assert.ok(
      key in histSnapshot,
      `Histogram should have the expected key: ${key}`
    );
    Assert.equal(
      histSnapshot[key].sum,
      value,
      `Should have counted the correct number of visits for ${key}`
    );
  }

  for (let [name, value] of Object.entries(expectedScalars)) {
    Assert.ok(name in scalars, `Scalar ${name} should have been added.`);
    Assert.deepEqual(
      scalars[name],
      value,
      `Should have counted the correct number of visits for ${name}`
    );
  }

  for (let name of SEARCH_AD_CLICK_SCALARS) {
    Assert.equal(
      name in scalars,
      name in expectedScalars,
      `Should have matched ${name} in scalars and expectedScalars`
    );
  }
}

async function searchInSearchbar(inputText, win = window) {
  await new Promise(r => waitForFocus(r, win));
  let sb = win.BrowserSearch.searchBar;
  // Write the search query in the searchbar.
  sb.focus();
  sb.value = inputText;
  sb.textbox.controller.startSearch(inputText);
  // Wait for the popup to show.
  await BrowserTestUtils.waitForEvent(sb.textbox.popup, "popupshown");
  // And then for the search to complete.
  await TestUtils.waitForCondition(
    () =>
      sb.textbox.controller.searchStatus >=
      Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH,
    "The search in the searchbar must complete."
  );
  return sb.textbox.popup;
}

function clearSearchbarHistory(win = window) {
  info("cleanup the search history");
  return FormHistory.update({ op: "remove", fieldname: "searchbar-history" });
}

/**
 * Checks that the recorded Glean impression event has an impression_id property
 * pointing to a valid UUID and that the impression_id is unique.
 *
 * @param {Array} recordedEvents The recorded Glean impression events whose
 * impression_id properties we need to verify.
 */
function assertUUIDs(recordedEvents) {
  const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
  let impressionIdsSet = new Set();

  for (let recordedEvent of recordedEvents) {
    let impressionId = recordedEvent.extra.impression_id;

    Assert.equal(
      typeof impressionId,
      "string",
      "should be an impression_id on the event"
    );
    Assert.ok(
      UUID_REGEX.test(impressionId),
      "impression_id should be a valid UUID"
    );

    Assert.ok(
      !impressionIdsSet.has(impressionId),
      "Should not have found a duplicate impression_id"
    );
    impressionIdsSet.add(impressionId);
  }
}

/**
 * Checks that we get the correct number of recorded Glean impression events
 * and that the recorded Glean impression events have the correct keys and
 * values.
 *
 * @param {Array} expectedEvents The expected impression events whose keys and
 * values we use to validate the recorded Glean impression events.
 */
function assertImpressionEvents(expectedEvents) {
  let recordedEvents = Glean.serp.impression.testGetValue();

  Assert.equal(
    recordedEvents.length,
    expectedEvents.length,
    "should have the correct number of Glean events"
  );

  for (let [idx, expectedEvent] of expectedEvents.entries()) {
    let recordedEvent = recordedEvents[idx].extra;
    for (let key of Object.keys(expectedEvent)) {
      Assert.equal(
        recordedEvent[key],
        expectedEvent[key],
        `the value for recorded key "${key}" should match the value for expected key "${key}"`
      );
    }
  }

  assertUUIDs(recordedEvents);
}
