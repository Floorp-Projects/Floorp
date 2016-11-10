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
  yield ContentTask.spawn(browser, { fieldName, text }, function* ({fieldName, text}) {
    // Avoid intermittent failures.
    if (fieldName === "searchText") {
      content.wrappedJSObject.gContentSearchController.remoteTimeout = 5000;
    }
    // Put the focus on the search box.
    let searchInput = content.document.getElementById(fieldName);
    searchInput.focus();
    searchInput.value = text;
  });
});
