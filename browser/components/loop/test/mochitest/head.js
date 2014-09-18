/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const HAWK_TOKEN_LENGTH = 64;
const {
  LOOP_SESSION_TYPE,
  MozLoopServiceInternal,
} = Cu.import("resource:///modules/loop/MozLoopService.jsm", {});

// Cache this value only once, at the beginning of a
// test run, so that it doesn't pick up the offline=true
// if offline mode is requested multiple times in a test run.
const WAS_OFFLINE = Services.io.offline;

var gMozLoopAPI;

function promiseGetMozLoopAPI() {
  let deferred = Promise.defer();
  let loopPanel = document.getElementById("loop-notification-panel");
  let btn = document.getElementById("loop-call-button");

  // Wait for the popup to be shown, then we can get the iframe and
  // wait for the iframe's load to be completed.
  loopPanel.addEventListener("popupshown", function onpopupshown() {
    loopPanel.removeEventListener("popupshown", onpopupshown, true);
    let iframe = document.getElementById(btn.getAttribute("notificationFrameId"));

    if (iframe.contentDocument &&
        iframe.contentDocument.readyState == "complete") {
      gMozLoopAPI = iframe.contentWindow.navigator.wrappedJSObject.mozLoop;

      deferred.resolve();
    } else {
      iframe.addEventListener("load", function panelOnLoad(e) {
        iframe.removeEventListener("load", panelOnLoad, true);

        gMozLoopAPI = iframe.contentWindow.navigator.wrappedJSObject.mozLoop;

        // We do this in an execute soon to allow any other event listeners to
        // be handled, just in case.
        deferred.resolve();
      }, true);
    }
  }, true);

  // Now we're setup, click the button.
  btn.click();

  // Remove the iframe after each test. This also avoids mochitest complaining
  // about leaks on shutdown as we intentionally hold the iframe open for the
  // life of the application.
  registerCleanupFunction(function() {
    loopPanel.hidePopup();
    let frameId = btn.getAttribute("notificationFrameId");
    let frame = document.getElementById(frameId);
    if (frame) {
      loopPanel.removeChild(frame);
    }
  });

  return deferred.promise;
}

/**
 * Loads the loop panel by clicking the button and waits for its open to complete.
 * It also registers
 *
 * This assumes that the tests are running in a generatorTest.
 */
function loadLoopPanel(aOverrideOptions = {}) {
  // Set prefs to ensure we don't access the network externally.
  Services.prefs.setCharPref("services.push.serverURL", aOverrideOptions.pushURL || "ws://localhost/");
  Services.prefs.setCharPref("loop.server", aOverrideOptions.loopURL || "http://localhost/");

  // Turn off the network for loop tests, so that we don't
  // try to access the remote servers. If we want to turn this
  // back on in future, be careful to check for intermittent
  // failures.
  if (!aOverrideOptions.stayOnline) {
    Services.io.offline = true;
  }

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("services.push.serverURL");
    Services.prefs.clearUserPref("loop.server");
    Services.io.offline = WAS_OFFLINE;
  });

  // Turn off animations to make tests quicker.
  let loopPanel = document.getElementById("loop-notification-panel");
  loopPanel.setAttribute("animate", "false");

  // Now get the actual API.
  yield promiseGetMozLoopAPI();
}

function promiseOAuthParamsSetup(baseURL, params) {
  let deferred = Promise.defer();
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
              createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("POST", baseURL + "/setup_params", true);
  xhr.setRequestHeader("X-Params", JSON.stringify(params));
  xhr.addEventListener("load", () => deferred.resolve(xhr));
  xhr.addEventListener("error", error => deferred.reject(error));
  xhr.send();

  return deferred.promise;
}

function resetFxA() {
  let global = Cu.import("resource:///modules/loop/MozLoopService.jsm", {});
  global.gHawkClient = null;
  global.gFxAOAuthClientPromise = null;
  global.gFxAOAuthClient = null;
  global.gFxAOAuthTokenData = null;
  global.gFxAOAuthProfile = null;
  const fxASessionPref = MozLoopServiceInternal.getSessionTokenPrefName(LOOP_SESSION_TYPE.FXA);
  Services.prefs.clearUserPref(fxASessionPref);
}

function setInternalLoopGlobal(aName, aValue) {
  let global = Cu.import("resource:///modules/loop/MozLoopService.jsm", {});
  global[aName] = aValue;
}

function promiseDeletedOAuthParams(baseURL) {
  let deferred = Promise.defer();
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
              createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("DELETE", baseURL + "/setup_params", true);
  xhr.addEventListener("load", () => deferred.resolve(xhr));
  xhr.addEventListener("error", deferred.reject);
  xhr.send();

  return deferred.promise;
}

function promiseObserverNotified(aTopic, aExpectedData = null) {
  let deferred = Promise.defer();
  Services.obs.addObserver(function onNotification(aSubject, aTopic, aData) {
    Services.obs.removeObserver(onNotification, aTopic);
    is(aData, aExpectedData, "observer data should match expected data")
    deferred.resolve({subject: aSubject, data: aData});
  }, aTopic, false);
  return deferred.promise;
}

/**
 * Get the last registration on the test server.
 */
function promiseOAuthGetRegistration(baseURL) {
  let deferred = Promise.defer();
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
              createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("GET", baseURL + "/get_registration", true);
  xhr.responseType = "json";
  xhr.addEventListener("load", () => deferred.resolve(xhr));
  xhr.addEventListener("error", deferred.reject);
  xhr.send();

  return deferred.promise;
}

/**
 * This is used to fake push registration and notifications for
 * MozLoopService tests. There is only one object created per test instance, as
 * once registration has taken place, the object cannot currently be changed.
 */
let mockPushHandler = {
  // This sets the registration result to be returned when initialize
  // is called. By default, it is equivalent to success.
  registrationResult: null,
  pushUrl: undefined,

  /**
   * MozLoopPushHandler API
   */
  initialize: function(registerCallback, notificationCallback) {
    registerCallback(this.registrationResult, this.pushUrl);
    this._notificationCallback = notificationCallback;
  },

  /**
   * Test-only API to simplify notifying a push notification result.
   */
  notify: function(version) {
    this._notificationCallback(version);
  }
};
