/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const HAWK_TOKEN_LENGTH = 64;
const {
  LOOP_SESSION_TYPE,
  MozLoopServiceInternal,
  MozLoopService,
} = Cu.import("resource:///modules/loop/MozLoopService.jsm", {});
const {LoopCalls} = Cu.import("resource:///modules/loop/LoopCalls.jsm", {});
const {LoopRooms} = Cu.import("resource:///modules/loop/LoopRooms.jsm", {});

// Cache this value only once, at the beginning of a
// test run, so that it doesn't pick up the offline=true
// if offline mode is requested multiple times in a test run.
const WAS_OFFLINE = Services.io.offline;


var gMozLoopAPI;

function promiseGetMozLoopAPI() {
  return new Promise((resolve, reject) => {
    let loopPanel = document.getElementById("loop-notification-panel");
    let btn = document.getElementById("loop-button");

    // Wait for the popup to be shown if it's not already, then we can get the iframe and
    // wait for the iframe's load to be completed.
    if (loopPanel.state == "closing" || loopPanel.state == "closed") {
      loopPanel.addEventListener("popupshown", () => {
        loopPanel.removeEventListener("popupshown", onpopupshown, true);
        onpopupshown();
      }, true);

      // Now we're setup, click the button.
      btn.click();
    } else {
      setTimeout(onpopupshown, 0);
    }

    function onpopupshown() {
      let iframe = document.getElementById(btn.getAttribute("notificationFrameId"));

      if (iframe.contentDocument &&
          iframe.contentDocument.readyState == "complete") {
        gMozLoopAPI = iframe.contentWindow.navigator.wrappedJSObject.mozLoop;

        resolve();
      } else {
        iframe.addEventListener("load", function panelOnLoad(e) {
          iframe.removeEventListener("load", panelOnLoad, true);

          gMozLoopAPI = iframe.contentWindow.navigator.wrappedJSObject.mozLoop;

          // We do this in an execute soon to allow any other event listeners to
          // be handled, just in case.
          resolve();
        }, true);
      }
    }

    // Remove the iframe after each test. This also avoids mochitest complaining
    // about leaks on shutdown as we intentionally hold the iframe open for the
    // life of the application.
    registerCleanupFunction(function() {
      loopPanel.hidePopup();
      let frameId = btn.getAttribute("notificationFrameId");
      let frame = document.getElementById(frameId);
      if (frame) {
        frame.remove();
      }
    });
  });
}

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

function promiseWaitForCondition(aConditionFn) {
  let deferred = Promise.defer();
  waitForCondition(aConditionFn, deferred.resolve, "Condition didn't pass.");
  return deferred.promise;
}

/**
 * Loads the loop panel by clicking the button and waits for its open to complete.
 * It also registers
 *
 * This assumes that the tests are running in a generatorTest.
 */
function loadLoopPanel(aOverrideOptions = {}) {
  // Turn off the network for loop tests, so that we don't
  // try to access the remote servers. If we want to turn this
  // back on in future, be careful to check for intermittent
  // failures.
  if (!aOverrideOptions.stayOnline) {
    Services.io.offline = true;
  }

  registerCleanupFunction(function() {
    Services.io.offline = WAS_OFFLINE;
  });

  // Turn off animations to make tests quicker.
  let loopPanel = document.getElementById("loop-notification-panel");
  loopPanel.setAttribute("animate", "false");

  // Now get the actual API loaded into gMozLoopAPI.
  return promiseGetMozLoopAPI();
}

function promiseOAuthParamsSetup(baseURL, params) {
  return new Promise((resolve, reject) => {
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("POST", baseURL + "/setup_params", true);
    xhr.setRequestHeader("X-Params", JSON.stringify(params));
    xhr.addEventListener("load", () => resolve(xhr));
    xhr.addEventListener("error", error => reject(error));
    xhr.send();
  });
}

function* resetFxA() {
  let global = Cu.import("resource:///modules/loop/MozLoopService.jsm", {});
  global.gHawkClient = null;
  global.gFxAOAuthClientPromise = null;
  global.gFxAOAuthClient = null;
  MozLoopServiceInternal.fxAOAuthProfile = null;
  MozLoopServiceInternal.fxAOAuthTokenData = null;
  const fxASessionPref = MozLoopServiceInternal.getSessionTokenPrefName(LOOP_SESSION_TYPE.FXA);
  Services.prefs.clearUserPref(fxASessionPref);
  MozLoopService.errors.clear();
  let notified = promiseObserverNotified("loop-status-changed");
  MozLoopServiceInternal.notifyStatusChanged();
  yield notified;
}

function checkFxAOAuthTokenData(aValue) {
  is(MozLoopServiceInternal.fxAOAuthTokenData, aValue, "fxAOAuthTokenData should be " + aValue);
}

function checkLoggedOutState() {
  let global = Cu.import("resource:///modules/loop/MozLoopService.jsm", {});
  is(global.gFxAOAuthClientPromise, null, "gFxAOAuthClientPromise should be cleared");
  is(MozLoopService.userProfile, null, "fxAOAuthProfile should be cleared");
  is(global.gFxAOAuthClient, null, "gFxAOAuthClient should be cleared");
  checkFxAOAuthTokenData(null);
  const fxASessionPref = MozLoopServiceInternal.getSessionTokenPrefName(LOOP_SESSION_TYPE.FXA);
  is(Services.prefs.getPrefType(fxASessionPref), Services.prefs.PREF_INVALID,
     "FxA hawk session should be cleared anyways");
}

function promiseDeletedOAuthParams(baseURL) {
  return new Promise((resolve, reject) => {
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("DELETE", baseURL + "/setup_params", true);
    xhr.addEventListener("load", () => resolve(xhr));
    xhr.addEventListener("error", reject);
    xhr.send();
  });
}

function promiseObserverNotified(aTopic, aExpectedData = null) {
  return new Promise((resolve, reject) => {
    Services.obs.addObserver(function onNotification(aSubject, aTopic, aData) {
      Services.obs.removeObserver(onNotification, aTopic);
      is(aData, aExpectedData, "observer data should match expected data");
      resolve({subject: aSubject, data: aData});
    }, aTopic, false);
  });
}

/**
 * Get the last registration on the test server.
 */
function promiseOAuthGetRegistration(baseURL) {
  return new Promise((resolve, reject) => {
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", baseURL + "/get_registration", true);
    xhr.responseType = "json";
    xhr.addEventListener("load", () => resolve(xhr));
    xhr.addEventListener("error", reject);
    xhr.send();
  });
}

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param tab
 *        The tab to load into.
 * @param [optional] url
 *        The url to load, or the current url.
 * @param [optional] event
 *        The load event type to wait for.  Defaults to "load".
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url, eventType="load") {
  return new Promise((resolve, reject) => {
    info("Wait tab event: " + eventType);

    function handle(event) {
      if (event.originalTarget != tab.linkedBrowser.contentDocument ||
          event.target.location.href == "about:blank" ||
          (url && event.target.location.href != url)) {
        info("Skipping spurious '" + eventType + "'' event" +
             " for " + event.target.location.href);
        return;
      }
      clearTimeout(timeout);
      tab.linkedBrowser.removeEventListener(eventType, handle, true);
      info("Tab event received: " + eventType);
      resolve(event);
    }

    let timeout = setTimeout(() => {
      if (tab.linkedBrowser) {
        tab.linkedBrowser.removeEventListener(eventType, handle, true);
      }
      reject(new Error("Timed out while waiting for a '" + eventType + "'' event"));
    }, 30000);

    tab.linkedBrowser.addEventListener(eventType, handle, true, true);
    if (url) {
      tab.linkedBrowser.loadURI(url);
    }
  });
}

function getLoopString(stringID) {
  return MozLoopServiceInternal.localizedStrings.get(stringID);
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
  registrationPushURLs: {},
  notificationCallback: {},
  registeredChannels: {},

  /**
   * MozLoopPushHandler API
   */
  initialize: function(options = {}) {
    if ("mockWebSocket" in options) {
      this._mockWebSocket = options.mockWebSocket;
    }
    this.registrationPushURLs[MozLoopService.channelIDs.callsGuest] =
      "https://localhost/pushUrl/guest-calls";
    this.registrationPushURLs[MozLoopService.channelIDs.roomsGuest] =
      "https://localhost/pushUrl/guest-rooms";
    this.registrationPushURLs[MozLoopService.channelIDs.callsFxA] =
      "https://localhost/pushUrl/fxa-calls";
    this.registrationPushURLs[MozLoopService.channelIDs.roomsFxA] =
      "https://localhost/pushUrl/fxa-rooms";
  },

  register: function(channelId, registerCallback, notificationCallback) {
    this.notificationCallback[channelId] = notificationCallback;
    this.registeredChannels[channelId] = this.registrationPushURLs[channelId];
    setTimeout(registerCallback(this.registrationResult, this.registeredChannels[channelId], channelId), 0);
  },

  unregister: function(channelID) {
    return;
  },

  /**
   * Test-only API to simplify notifying a push notification result.
   */
  notify: function(version, chanId) {
    this.notificationCallback[chanId](version, chanId);
  }
};

const mockDb = {
  _store: { },
  _next_guid: 1,

  get size() {
    return Object.getOwnPropertyNames(this._store).length;
  },

  add: function(details, callback) {
    if (!("id" in details)) {
      callback(new Error("No 'id' field present"));
      return;
    }
    details._guid = this._next_guid++;
    this._store[details._guid] = details;
    callback(null, details);
  },
  remove: function(guid, callback) {
    if (!(guid in this._store)) {
      callback(new Error("Could not find _guid '" + guid + "' in database"));
      return;
    }
    delete this._store[guid];
    callback(null);
  },
  getAll: function(callback) {
    callback(null, this._store);
  },
  get: function(guid, callback) {
    callback(null, this._store[guid]);
  },
  getByServiceId: function(serviceId, callback) {
    for (let guid in this._store) {
      if (serviceId === this._store[guid].id) {
        callback(null, this._store[guid]);
        return;
      }
    }
    callback(null, null);
  },
  removeAll: function(callback) {
    this._store = {};
    this._next_guid = 1;
    callback(null);
  },
  promise: function(method, ...params) {
    return new Promise(resolve => {
      this[method](...params, (err, res) => err ? reject(err) : resolve(res));
    });
  }
};
