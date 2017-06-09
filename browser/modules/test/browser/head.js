
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
                                  "resource://testing-common/PlacesTestUtils.jsm");

const SINGLE_TRY_TIMEOUT = 100;
const NUMBER_OF_TRIES = 30;

function waitForConditionPromise(condition, timeoutMsg, tryCount = NUMBER_OF_TRIES) {
  return new Promise((resolve, reject) => {
    let tries = 0;
    function checkCondition() {
      if (tries >= tryCount) {
        reject(timeoutMsg);
      }
      var conditionPassed;
      try {
        conditionPassed = condition();
      } catch (e) {
        return reject(e);
      }
      if (conditionPassed) {
        return resolve();
      }
      tries++;
      setTimeout(checkCondition, SINGLE_TRY_TIMEOUT);
      return undefined;
    }
    setTimeout(checkCondition, SINGLE_TRY_TIMEOUT);
  });
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
let typeInSearchField = async function(browser, text, fieldName) {
  await ContentTask.spawn(browser, [fieldName, text], async function([contentFieldName, contentText]) {
    // Put the focus on the search box.
    let searchInput = content.document.getElementById(contentFieldName);
    searchInput.focus();
    searchInput.value = contentText;
  });
};


/**
 * Clear and get the named histogram
 * @param {String} name
 *        The name of the histogram
 */
function getAndClearHistogram(name) {
  let histogram = Services.telemetry.getHistogramById(name);
  histogram.clear();
  return histogram;
}


/**
 * Clear and get the named keyed histogram
 * @param {String} name
 *        The name of the keyed histogram
 */
function getAndClearKeyedHistogram(name) {
  let histogram = Services.telemetry.getKeyedHistogramById(name);
  histogram.clear();
  return histogram;
}


/**
 * Check that the keyed histogram contains the right value.
 */
function checkKeyedHistogram(h, key, expectedValue) {
  const snapshot = h.snapshot();
  Assert.ok(key in snapshot, `The histogram must contain ${key}.`);
  Assert.equal(snapshot[key].sum, expectedValue, `The key ${key} must contain ${expectedValue}.`);
}

/**
 * Return the scalars from the parent-process.
 */
function getParentProcessScalars(aChannel, aKeyed = false, aClear = false) {
  const scalars = aKeyed ?
    Services.telemetry.snapshotKeyedScalars(aChannel, aClear)["parent"] :
    Services.telemetry.snapshotScalars(aChannel, aClear)["parent"];
  return scalars || {};
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

/**
 * Given a <xul:browser> at some non-internal web page,
 * return something that resembles an nsIContentPermissionRequest,
 * using the browsers currently loaded document to get a principal.
 *
 * @param browser (<xul:browser>)
 *        The browser that we'll create a nsIContentPermissionRequest
 *        for.
 * @returns A nsIContentPermissionRequest-ish object.
 */
function makeMockPermissionRequest(browser) {
  let result = {
    types: null,
    principal: browser.contentPrincipal,
    requester: null,
    _cancelled: false,
    cancel() {
      this._cancelled = true;
    },
    _allowed: false,
    allow() {
      this._allowed = true;
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionRequest]),
  };

  // In the e10s-case, nsIContentPermissionRequest will have
  // element defined. window is defined otherwise.
  if (browser.isRemoteBrowser) {
    result.element = browser;
  } else {
    result.window = browser.contentWindow;
  }

  return result;
}

/**
 * For an opened PopupNotification, clicks on the main action,
 * and waits for the panel to fully close.
 *
 * @return {Promise}
 *         Resolves once the panel has fired the "popuphidden"
 *         event.
 */
function clickMainAction() {
  let removePromise =
    BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
  let popupNotification = getPopupNotificationNode();
  popupNotification.button.click();
  return removePromise;
}

/**
 * For an opened PopupNotification, clicks on the secondary action,
 * and waits for the panel to fully close.
 *
 * @param actionIndex (Number)
 *        The index of the secondary action to be clicked. The default
 *        secondary action (the button shown directly in the panel) is
 *        treated as having index 0.
 *
 * @return {Promise}
 *         Resolves once the panel has fired the "popuphidden"
 *         event.
 */
function clickSecondaryAction(actionIndex) {
  let removePromise =
    BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
  let popupNotification = getPopupNotificationNode();
  if (!actionIndex) {
    popupNotification.secondaryButton.click();
    return removePromise;
  }

  return (async function() {
    // Click the dropmarker arrow and wait for the menu to show up.
    let dropdownPromise =
      BrowserTestUtils.waitForEvent(popupNotification.menupopup, "popupshown");
    await EventUtils.synthesizeMouseAtCenter(popupNotification.menubutton, {});
    await dropdownPromise;

    // The menuitems in the dropdown are accessible as direct children of the panel,
    // because they are injected into a <children> node in the XBL binding.
    // The target action is the menuitem at index actionIndex - 1, because the first
    // secondary action (index 0) is the button shown directly in the panel.
    let actionMenuItem = popupNotification.querySelectorAll("menuitem")[actionIndex - 1];
    await EventUtils.synthesizeMouseAtCenter(actionMenuItem, {});
    await removePromise;
  })();
}

/**
 * Makes sure that 1 (and only 1) <xul:popupnotification> is being displayed
 * by PopupNotification, and then returns that <xul:popupnotification>.
 *
 * @return {<xul:popupnotification>}
 */
function getPopupNotificationNode() {
  // PopupNotification is a bit overloaded here, so to be
  // clear, popupNotifications is a list of <xul:popupnotification>
  // nodes.
  let popupNotifications = PopupNotifications.panel.childNodes;
  Assert.equal(popupNotifications.length, 1,
               "Should be showing a <xul:popupnotification>");
  return popupNotifications[0];
}

