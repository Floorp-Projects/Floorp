Cu.import("resource://gre/modules/Promise.jsm");

const SINGLE_TRY_TIMEOUT = 100;
const NUMBER_OF_TRIES = 30;

function waitForConditionPromise(condition, timeoutMsg, tryCount = NUMBER_OF_TRIES) {
  let defer = Promise.defer();
  let tries = 0;
  function checkCondition() {
    if (tries >= tryCount) {
      defer.reject(timeoutMsg);
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      return defer.reject(e);
    }
    if (conditionPassed) {
      return defer.resolve();
    }
    tries++;
    setTimeout(checkCondition, SINGLE_TRY_TIMEOUT);
    return undefined;
  }
  setTimeout(checkCondition, SINGLE_TRY_TIMEOUT);
  return defer.promise;
}

function waitForCondition(condition, nextTest, errorMsg) {
  waitForConditionPromise(condition, errorMsg).then(nextTest, (reason) => {
    ok(false, reason + (reason.stack ? "\n" + reason.stack : ""));
  });
}

/**
 * Checks if the snapshotted keyed scalars contain the expected
 * data.
 *
 * @param {Object} scalars
 *        The snapshot of the keyed scalars.
 * @param {String} scalarName
 *        The name of the keyed scalar to check.
 * @param {String} key
 *        The key that must be within the keyed scalar.
 * @param {String|Boolean|Number} expectedValue
 *        The expected value for the provided key in the scalar.
 */
function checkKeyedScalar(scalars, scalarName, key, expectedValue) {
  Assert.ok(scalarName in scalars,
            scalarName + " must be recorded.");
  Assert.ok(key in scalars[scalarName],
            scalarName + " must contain the '" + key + "' key.");
  Assert.ok(scalars[scalarName][key], expectedValue,
            scalarName + "['" + key + "'] must contain the expected value");
}

/**
 * An utility function to write some text in the search input box
 * in a content page.
 * @param {Object} browser
 *        The browser that contains the content.
 * @param {String} text
 *        The string to write in the search field.
 * @param {String} fieldName
 *        The name of the field to write to.
 */
let typeInSearchField = Task.async(function* (browser, text, fieldName) {
  yield ContentTask.spawn(browser, [fieldName, text], function* ([contentFieldName, contentText]) {
    // Avoid intermittent failures.
    if (contentFieldName === "searchText") {
      content.wrappedJSObject.gContentSearchController.remoteTimeout = 5000;
    }
    // Put the focus on the search box.
    let searchInput = content.document.getElementById(contentFieldName);
    searchInput.focus();
    searchInput.value = contentText;
  });
});

/**
 * Clear and get the SEARCH_COUNTS histogram.
 */
function getSearchCountsHistogram() {
  let search_hist = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
  search_hist.clear();
  return search_hist;
}

/**
 * Check that the keyed histogram contains the right value.
 */
function checkKeyedHistogram(h, key, expectedValue) {
  const snapshot = h.snapshot();
  Assert.ok(key in snapshot, `The histogram must contain ${key}.`);
  Assert.equal(snapshot[key].sum, expectedValue, `The key ${key} must contain ${expectedValue}.`);
}

function checkEvents(events, expectedEvents) {
  if (!Services.telemetry.canRecordExtended) {
    // Currently we only collect the tested events when extended Telemetry is enabled.
    return;
  }

  Assert.equal(events.length, expectedEvents.length, "Should have matching amount of events.");

  // Strip timestamps from the events for easier comparison.
  events = events.map(e => e.slice(1));

  for (let i = 0; i < events.length; ++i) {
    Assert.deepEqual(events[i], expectedEvents[i], "Events should match.");
  }
}
