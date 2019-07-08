ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryTestUtils",
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const SINGLE_TRY_TIMEOUT = 100;
const NUMBER_OF_TRIES = 30;

function waitForConditionPromise(
  condition,
  timeoutMsg,
  tryCount = NUMBER_OF_TRIES
) {
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
  waitForConditionPromise(condition, errorMsg).then(nextTest, reason => {
    ok(false, reason + (reason.stack ? "\n" + reason.stack : ""));
  });
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
  await ContentTask.spawn(browser, [fieldName, text], async function([
    contentFieldName,
    contentText,
  ]) {
    // Put the focus on the search box.
    let searchInput = content.document.getElementById(contentFieldName);
    searchInput.focus();
    searchInput.value = contentText;
  });
};

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
  let type = {
    options: Cc["@mozilla.org/array;1"].createInstance(Ci.nsIArray),
    QueryInterface: ChromeUtils.generateQI([Ci.nsIContentPermissionType]),
  };
  let types = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  types.appendElement(type);
  let result = {
    types,
    documentDOMContentLoadedTimestamp: 0,
    isHandlingUserInput: false,
    userHadInteractedWithDocument: false,
    principal: browser.contentPrincipal,
    topLevelPrincipal: browser.contentPrincipal,
    requester: null,
    _cancelled: false,
    cancel() {
      this._cancelled = true;
    },
    _allowed: false,
    allow() {
      this._allowed = true;
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsIContentPermissionRequest]),
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
  let removePromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );
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
  let removePromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );
  let popupNotification = getPopupNotificationNode();
  if (!actionIndex) {
    popupNotification.secondaryButton.click();
    return removePromise;
  }

  return (async function() {
    // Click the dropmarker arrow and wait for the menu to show up.
    let dropdownPromise = BrowserTestUtils.waitForEvent(
      popupNotification.menupopup,
      "popupshown"
    );
    await EventUtils.synthesizeMouseAtCenter(popupNotification.menubutton, {});
    await dropdownPromise;

    // The menuitems in the dropdown are accessible as direct children of the panel,
    // because they are injected into a <children> node in the XBL binding.
    // The target action is the menuitem at index actionIndex - 1, because the first
    // secondary action (index 0) is the button shown directly in the panel.
    let actionMenuItem = popupNotification.querySelectorAll("menuitem")[
      actionIndex - 1
    ];
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
  Assert.equal(
    popupNotifications.length,
    1,
    "Should be showing a <xul:popupnotification>"
  );
  return popupNotifications[0];
}

/**
 * Disable non-release page actions (that are tested elsewhere).
 *
 * @return void
 */
async function disableNonReleaseActions() {
  if (!["release", "esr"].includes(AppConstants.MOZ_UPDATE_CHANNEL)) {
    SpecialPowers.Services.prefs.setBoolPref(
      "extensions.webcompat-reporter.enabled",
      false
    );
  }
}
