/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_CONTEXT = "chrome";

const SETTINGS_KEY_DATA_ENABLED = "ril.data.enabled";
const SETTINGS_KEY_DATA_APN_SETTINGS  = "ril.data.apnSettings";
const SETTINGS_KEY_WIFI_ENABLED = "wifi.enabled";

const TOPIC_CONNECTION_STATE_CHANGED = "network-connection-state-changed";
const TOPIC_NETWORK_ACTIVE_CHANGED = "network-active-changed";

const NETWORK_STATE_UNKNOWN = Ci.nsINetworkInfo.NETWORK_STATE_UNKNOWN;
const NETWORK_STATE_CONNECTING = Ci.nsINetworkInfo.NETWORK_STATE_CONNECTING;
const NETWORK_STATE_CONNECTED = Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED;
const NETWORK_STATE_DISCONNECTING = Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTING;
const NETWORK_STATE_DISCONNECTED = Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED;

const NETWORK_TYPE_MOBILE = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE;
const NETWORK_TYPE_MOBILE_MMS = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_MMS;
const NETWORK_TYPE_MOBILE_SUPL = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_SUPL;
const NETWORK_TYPE_MOBILE_IMS = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_IMS;
const NETWORK_TYPE_MOBILE_DUN = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN;
const NETWORK_TYPE_MOBILE_FOTA = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_FOTA;

const networkTypes = [
  NETWORK_TYPE_MOBILE,
  NETWORK_TYPE_MOBILE_MMS,
  NETWORK_TYPE_MOBILE_SUPL,
  NETWORK_TYPE_MOBILE_IMS,
  NETWORK_TYPE_MOBILE_DUN,
  NETWORK_TYPE_MOBILE_FOTA
];

var Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

var ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
ok(ril, "ril.constructor is " + ril.constructor);

var radioInterface = ril.getRadioInterface(0);
ok(radioInterface, "radioInterface.constructor is " + radioInterface.constrctor);

var _pendingEmulatorShellCmdCount = 0;
var _pendingEmulatorCmdCount = 0;

/**
 * Send emulator shell command with safe guard.
 *
 * We should only call |finish()| after all emulator shell command transactions
 * end, so here comes with the pending counter.  Resolve when the emulator
 * shell gives response. Never reject.
 *
 * Fulfill params:
 *   result -- an array of emulator shell response lines.
 *
 * @param aCommands
 *        A string array commands to be passed to emulator through adb shell.
 *
 * @return A deferred promise.
 */
function runEmulatorShellCmdSafe(aCommands) {
  return new Promise(function(aResolve, aReject) {
    ++_pendingEmulatorShellCmdCount;
    runEmulatorShell(aCommands, function(aResult) {
      --_pendingEmulatorShellCmdCount;

      log("Emulator shell response: " + JSON.stringify(aResult));
      aResolve(aResult);
    });
  });
}

/**
 * Send emulator command with safe guard.
 *
 * We should only call |finish()| after all emulator command transactions
 * end, so here comes with the pending counter.  Resolve when the emulator
 * gives positive response, and reject otherwise.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @param aCommand
 *        A string command to be passed to emulator through its telnet console.
 *
 * @return A deferred promise.
 */
function runEmulatorCmdSafe(aCommand) {
  log(aCommand);
  return new Promise(function(aResolve, aReject) {
    ++_pendingEmulatorCmdCount;
    runEmulatorCmd(aCommand, function(aResult) {
      --_pendingEmulatorCmdCount;

      log("Emulator console response: " + JSON.stringify(aResult));
      if (Array.isArray(aResult) &&
          aResult[aResult.length - 1] === "OK") {
        aResolve(aResult);
      } else {
        aReject(aResult);
      }
    });
  });
}

/**
 * Get mozSettings value specified by @aKey.
 *
 * Resolve if that mozSettings value is retrieved successfully, reject
 * otherwise.
 *
 * Fulfill params: The corresponding mozSettings value of the key.
 * Reject params: (none)
 *
 * @param aKey
 *        A string.
 * @param aAllowError [optional]
 *        A boolean value.  If set to true, an error response won't be treated
 *        as test failure.  Default: false.
 *
 * @return A deferred promise.
 */
function getSettings(aKey, aAllowError) {
  let request = window.navigator.mozSettings.createLock().get(aKey);
  return request.then(function resolve(aValue) {
      log("getSettings(" + aKey + ") - success");
      return aValue[aKey];
    }, function reject(aError) {
      ok(aAllowError, "getSettings(" + aKey + ") - error");
    });
}

/**
 * Set mozSettings values.
 *
 * Resolve if that mozSettings value is set successfully, reject otherwise.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @param aKey
 *        A string key.
 * @param aValue
 *        An object value.
 * @param aAllowError [optional]
 *        A boolean value.  If set to true, an error response won't be treated
 *        as test failure.  Default: false.
 *
 * @return A deferred promise.
 */
function setSettings(aKey, aValue, aAllowError) {
  let settings = {};
  settings[aKey] = aValue;
  let lock = window.navigator.mozSettings.createLock();
  let request = lock.set(settings);
  let deferred = Promise.defer();
  lock.onsettingstransactionsuccess = function () {
      log("setSettings(" + JSON.stringify(settings) + ") - success");
    deferred.resolve();
  };
  lock.onsettingstransactionfailure = function () {
      ok(aAllowError, "setSettings(" + JSON.stringify(settings) + ") - error");
    // We resolve even though we've thrown an error, since the ok()
    // will do that.
    deferred.resolve();
  };
  return deferred.promise;
}

/**
 * Wait for observer event.
 *
 * Resolve if that topic event occurs.  Never reject.
 *
 * Fulfill params: the subject passed.
 *
 * @param aTopic
 *        A string topic name.
 *
 * @return A deferred promise.
 */
function waitForObserverEvent(aTopic) {
  let obs = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  let deferred = Promise.defer();

  obs.addObserver(function observer(subject, topic, data) {
    if (topic === aTopic) {
      obs.removeObserver(observer, aTopic);
      deferred.resolve(subject);
    }
  }, aTopic);

  return deferred.promise;
}

/**
 * Wait for one named event.
 *
 * Resolve if that named event occurs.  Never reject.
 *
 * Fulfill params: the DOMEvent passed.
 *
 * @param aEventTarget
 *        An EventTarget object.
 * @param aEventName
 *        A string event name.
 * @param aMatchFun [optional]
 *        A matching function returns true or false to filter the event.
 *
 * @return A deferred promise.
 */
function waitForTargetEvent(aEventTarget, aEventName, aMatchFun) {
  return new Promise(function(aResolve, aReject) {
    aEventTarget.addEventListener(aEventName, function onevent(aEvent) {
      if (!aMatchFun || aMatchFun(aEvent)) {
        aEventTarget.removeEventListener(aEventName, onevent);
        ok(true, "Event '" + aEventName + "' got.");
        aResolve(aEvent);
      }
    });
  });
}

/**
 * Set the default data connection enabling state, wait for
 * "network-connection-state-changed" event and verify state.
 *
 * Fulfill params: instance of nsIRilNetworkInfo of the network connected.
 *
 * @param aEnabled
 *        A boolean state.
 *
 * @return A deferred promise.
 */
function setDataEnabledAndWait(aEnabled) {
  let promises = [];
  promises.push(waitForObserverEvent(TOPIC_CONNECTION_STATE_CHANGED)
    .then(function(aSubject) {
      ok(aSubject instanceof Ci.nsIRilNetworkInfo,
         "subject should be an instance of nsIRilNetworkInfo");
      is(aSubject.type, NETWORK_TYPE_MOBILE,
         "subject.type should be " + NETWORK_TYPE_MOBILE);
      is(aSubject.state,
         aEnabled ? Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
                  : Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED,
         "subject.state should be " + aEnabled ? "CONNECTED" : "DISCONNECTED");

      return aSubject;
    }));
  promises.push(setSettings(SETTINGS_KEY_DATA_ENABLED, aEnabled));

  return Promise.all(promises).then(aValues => aValues[0]);
}

/**
 * Setup a certain type of data connection, wait for
 * "network-connection-state-changed" event and verify state.
 *
 * Fulfill params: instance of nsIRilNetworkInfo of the network connected.
 *
 * @param aNetworkType
 *        The mobile network type to setup.
 *
 * @return A deferred promise.
 */
function setupDataCallAndWait(aNetworkType) {
  log("setupDataCallAndWait: " + aNetworkType);

  let promises = [];
  promises.push(waitForObserverEvent(TOPIC_CONNECTION_STATE_CHANGED)
    .then(function(aSubject) {
      ok(aSubject instanceof Ci.nsIRilNetworkInfo,
         "subject should be an instance of nsIRilNetworkInfo");
      is(aSubject.type, aNetworkType,
         "subject.type should be " + aNetworkType);
      is(aSubject.state, Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED,
         "subject.state should be CONNECTED");

      return aSubject;
    }));
  promises.push(radioInterface.setupDataCallByType(aNetworkType));

  return Promise.all(promises).then(aValues => aValues[0]);
}

/**
 * Deactivate a certain type of data connection, wait for
 * "network-connection-state-changed" event and verify state.
 *
 * Fulfill params: (none)
 *
 * @param aNetworkType
 *        The mobile network type to deactivate.
 *
 * @return A deferred promise.
 */
function deactivateDataCallAndWait(aNetworkType) {
  log("deactivateDataCallAndWait: " + aNetworkType);

  let promises = [];
  promises.push(waitForObserverEvent(TOPIC_CONNECTION_STATE_CHANGED)
    .then(function(aSubject) {
      ok(aSubject instanceof Ci.nsIRilNetworkInfo,
         "subject should be an instance of nsIRilNetworkInfo");
      is(aSubject.type, aNetworkType,
         "subject.type should be " + aNetworkType);
      is(aSubject.state, Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED,
         "subject.state should be DISCONNECTED");
    }));
  promises.push(radioInterface.deactivateDataCallByType(aNetworkType));

  return Promise.all(promises);
}

/**
 * Wait for pending emulator transactions and call |finish()|.
 */
function cleanUp() {
  // Use ok here so that we have at least one test run.
  ok(true, ":: CLEANING UP ::");

  waitFor(finish, function() {
    return _pendingEmulatorShellCmdCount === 0 &&
           _pendingEmulatorCmdCount === 0;
  });
}

/**
 * Basic test routine helper.
 *
 * This helper does nothing but clean-ups.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 */
function startTestBase(aTestCaseMain) {
  Promise.resolve()
    .then(aTestCaseMain)
    .then(cleanUp, function(aException) {
      ok(false, "promise rejects during test: " + aException);
      cleanUp();
    });
}
