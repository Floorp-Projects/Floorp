/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

const SETTINGS_KEY_DATA_ENABLED = "ril.data.enabled";
const SETTINGS_KEY_DATA_ROAMING_ENABLED = "ril.data.roaming_enabled";
const SETTINGS_KEY_DATA_APN_SETTINGS = "ril.data.apnSettings";

const PREF_KEY_RIL_DEBUGGING_ENABLED = "ril.debugging.enabled";

// The pin code hard coded in emulator is "0000".
const DEFAULT_PIN = "0000";
// The puk code hard coded in emulator is "12345678".
const DEFAULT_PUK = "12345678";

// Emulate Promise.jsm semantics.
Promise.defer = function() { return new Deferred(); };
function Deferred() {
  this.promise = new Promise(function(resolve, reject) {
    this.resolve = resolve;
    this.reject = reject;
  }.bind(this));
  Object.freeze(this);
}

let _pendingEmulatorCmdCount = 0;
let _pendingEmulatorShellCmdCount = 0;

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
  log("Emulator command: " + aCommand);
  let deferred = Promise.defer();

  ++_pendingEmulatorCmdCount;
  runEmulatorCmd(aCommand, function(aResult) {
    --_pendingEmulatorCmdCount;

    log("Emulator response: " + JSON.stringify(aResult));
    if (Array.isArray(aResult) &&
        aResult[aResult.length - 1] === "OK") {
      deferred.resolve(aResult);
    } else {
      deferred.reject(aResult);
    }
  });

  return deferred.promise;
}

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
  let deferred = Promise.defer();

  ++_pendingEmulatorShellCmdCount;
  runEmulatorShell(aCommands, function(aResult) {
    --_pendingEmulatorShellCmdCount;

    log("Emulator shell response: " + JSON.stringify(aResult));
    deferred.resolve(aResult);
  });

  return deferred.promise;
}

let workingFrame;

/**
 * Get mozSettings value specified by @aKey.
 *
 * Resolve if that mozSettings value is retrieved successfully, reject
 * otherwise.
 *
 * Fulfill params:
 *   The corresponding mozSettings value of the key.
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
  let request =
    workingFrame.contentWindow.navigator.mozSettings.createLock().get(aKey);
  return request.then(function resolve(aValue) {
      ok(true, "getSettings(" + aKey + ") - success");
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
 * @param aSettings
 *        An object of format |{key1: value1, key2: value2, ...}|.
 * @param aAllowError [optional]
 *        A boolean value.  If set to true, an error response won't be treated
 *        as test failure.  Default: false.
 *
 * @return A deferred promise.
 */

function setSettings(aSettings, aAllowError) {
  let lock = window.navigator.mozSettings.createLock();
  let request = lock.set(aSettings);
  let deferred = Promise.defer();
  lock.onsettingstransactionsuccess = function () {
      ok(true, "setSettings(" + JSON.stringify(aSettings) + ")");
    deferred.resolve();
  };
  lock.onsettingstransactionfailure = function (aEvent) {
      ok(aAllowError, "setSettings(" + JSON.stringify(aSettings) + ")");
    deferred.reject();
  };
  return deferred.promise;
}

/**
 * Set mozSettings value with only one key.
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
function setSettings1(aKey, aValue, aAllowError) {
  let settings = {};
  settings[aKey] = aValue;
  return setSettings(settings, aAllowError);
}

/**
 * Convenient MozSettings getter for SETTINGS_KEY_DATA_ENABLED.
 */
function getDataEnabled(aAllowError) {
  return getSettings(SETTINGS_KEY_DATA_ENABLED, aAllowError);
}

/**
 * Convenient MozSettings setter for SETTINGS_KEY_DATA_ENABLED.
 */
function setDataEnabled(aEnabled, aAllowError) {
  return setSettings1(SETTINGS_KEY_DATA_ENABLED, aEnabled, aAllowError);
}

/**
 * Convenient MozSettings getter for SETTINGS_KEY_DATA_ROAMING_ENABLED.
 */
function getDataRoamingEnabled(aAllowError) {
  return getSettings(SETTINGS_KEY_DATA_ROAMING_ENABLED, aAllowError);
}

/**
 * Convenient MozSettings setter for SETTINGS_KEY_DATA_ROAMING_ENABLED.
 */
function setDataRoamingEnabled(aEnabled, aAllowError) {
  return setSettings1(SETTINGS_KEY_DATA_ROAMING_ENABLED, aEnabled, aAllowError);
}

/**
 * Convenient MozSettings getter for SETTINGS_KEY_DATA_APN_SETTINGS.
 */
function getDataApnSettings(aAllowError) {
  return getSettings(SETTINGS_KEY_DATA_APN_SETTINGS, aAllowError);
}

/**
 * Convenient MozSettings setter for SETTINGS_KEY_DATA_APN_SETTINGS.
 */
function setDataApnSettings(aApnSettings, aAllowError) {
  return setSettings1(SETTINGS_KEY_DATA_APN_SETTINGS, aApnSettings, aAllowError);
}

let mobileConnection;

/**
 * Push required permissions and test if
 * |navigator.mozMobileConnections[<aServiceId>]| exists. Resolve if it does,
 * reject otherwise.
 *
 * Fulfill params:
 *   mobileConnection -- an reference to navigator.mozMobileMessage.
 *
 * Reject params: (none)
 *
 * @param aAdditonalPermissions [optional]
 *        An array of permission strings other than "mobileconnection" to be
 *        pushed. Default: empty string.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: 0.
 *
 * @return A deferred promise.
 */
function ensureMobileConnection(aAdditionalPermissions, aServiceId) {
  let deferred = Promise.defer();

  aAdditionalPermissions = aAdditionalPermissions || [];
  aServiceId = aServiceId || 0;

  if (aAdditionalPermissions.indexOf("mobileconnection") < 0) {
    aAdditionalPermissions.push("mobileconnection");
  }
  let permissions = [];
  for (let perm of aAdditionalPermissions) {
    permissions.push({ "type": perm, "allow": 1, "context": document });
  }

  SpecialPowers.pushPermissions(permissions, function() {
    ok(true, "permissions pushed: " + JSON.stringify(permissions));

    // Permission changes can't change existing Navigator.prototype
    // objects, so grab our objects from a new Navigator.
    workingFrame = document.createElement("iframe");
    workingFrame.addEventListener("load", function load() {
      workingFrame.removeEventListener("load", load);

      mobileConnection =
        workingFrame.contentWindow.navigator.mozMobileConnections[aServiceId];

      if (mobileConnection) {
        log("navigator.mozMobileConnections[" + aServiceId + "] is instance of " +
            mobileConnection.constructor);
      } else {
        log("navigator.mozMobileConnections[" + aServiceId + "] is undefined");
      }

      if (mobileConnection instanceof MozMobileConnection) {
        deferred.resolve(mobileConnection);
      } else {
        deferred.reject();
      }
    });

    document.body.appendChild(workingFrame);
  });

  return deferred.promise;
}

/**
 * Get MozMobileConnection by ServiceId
 *
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A MozMobileConnection.
 */
function getMozMobileConnectionByServiceId(aServiceId) {
  let mobileConn = mobileConnection;
  if (aServiceId !== undefined) {
    mobileConn =
      workingFrame.contentWindow.navigator.mozMobileConnections[aServiceId];
  }
  return mobileConn;
}

/**
 * Get MozIccManager
 *
 * @return a MozIccManager
 */
function getMozIccManager() {
  return workingFrame.contentWindow.navigator.mozIccManager;
}

/**
 * Get MozIcc by IccId
 *
 * @param aIccId [optional]
 *        Default: The first item of |mozIccManager.iccIds|.
 *
 * @return A MozIcc.
 */
function getMozIccByIccId(aIccId) {
  let iccManager = getMozIccManager();

  aIccId = aIccId || iccManager.iccIds[0];
  if (!aIccId) {
    ok(true, "iccManager.iccIds[0] is " + aIccId);
    return null;
  }

  return iccManager.getIccById(aIccId);
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
  let deferred = Promise.defer();

  aEventTarget.addEventListener(aEventName, function onevent(aEvent) {
    if (!aMatchFun || aMatchFun(aEvent)) {
      aEventTarget.removeEventListener(aEventName, onevent);
      ok(true, "Event '" + aEventName + "' got.");
      deferred.resolve(aEvent);
    }
  });

  return deferred.promise;
}

/**
 * Wait for one named MobileConnection event.
 *
 * Resolve if that named event occurs.  Never reject.
 *
 * Fulfill params: the DOMEvent passed.
 *
 * @param aEventName
 *        A string event name.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 * @param aMatchFun [optional]
 *        A matching function returns true or false to filter the event.
 *
 * @return A deferred promise.
 */
function waitForManagerEvent(aEventName, aServiceId, aMatchFun) {
  let mobileConn = getMozMobileConnectionByServiceId(aServiceId);
  return waitForTargetEvent(mobileConn, aEventName, aMatchFun);
}

/**
 * Get available networks.
 *
 * Fulfill params:
 *   An array of MozMobileNetworkInfo.
 * Reject params:
 *   A DOMEvent.
 *
 * @return A deferred promise.
 */
function getNetworks() {
  let request = mobileConnection.getNetworks();
  return request.then(() => request.result);
}

/**
 * Manually select a network.
 *
 * Fulfill params: (none)
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'
 *
 * @param aNetwork
 *        A MozMobileNetworkInfo.
 *
 * @return A deferred promise.
 */
function selectNetwork(aNetwork) {
  let request = mobileConnection.selectNetwork(aNetwork);
  return request.then(null, () => { throw request.error });
}

/**
 * Manually select a network and wait for a 'voicechange' event.
 *
 * Fulfill params: (none)
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'
 *
 * @param aNetwork
 *        A MozMobileNetworkInfo.
 *
 * @return A deferred promise.
 */
function selectNetworkAndWait(aNetwork) {
  let promises = [];

  promises.push(waitForManagerEvent("voicechange"));
  promises.push(selectNetwork(aNetwork));

  return Promise.all(promises);
}

/**
 * Automatically select a network.
 *
 * Fulfill params: (none)
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'
 *
 * @return A deferred promise.
 */
function selectNetworkAutomatically() {
  let request = mobileConnection.selectNetworkAutomatically();
  return request.then(null, () => { throw request.error });
}

/**
 * Automatically select a network and wait for a 'voicechange' event.
 *
 * Fulfill params: (none)
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'
 *
 * @return A deferred promise.
 */
function selectNetworkAutomaticallyAndWait() {
  let promises = [];

  promises.push(waitForManagerEvent("voicechange"));
  promises.push(selectNetworkAutomatically());

  return Promise.all(promises);
}

/**
 * Set roaming preference.
 *
 * Fulfill params: (none)
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'.
 *
 * @param aMode
 *        'home', 'affiliated', or 'any'.
 *
 * @return A deferred promise.
 */
 function setRoamingPreference(aMode) {
  let request = mobileConnection.setRoamingPreference(aMode);
  return request.then(null, () => { throw request.error });
}

/**
 * Set preferred network type.
 *
 * Fulfill params: (none)
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', 'ModeNotSupported' or
 *   'GenericFailure'.
 *
 * @param aType
 *        'wcdma/gsm', 'gsm', 'wcdma', 'wcdma/gsm-auto', 'cdma/evdo', 'cdma',
 *        'evdo', 'wcdma/gsm/cdma/evdo', 'lte/cdma/evdo', 'lte/wcdma/gsm',
 *        'lte/wcdma/gsm/cdma/evdo' or 'lte'.
 *
 * @return A deferred promise.
 */
 function setPreferredNetworkType(aType) {
  let request = mobileConnection.setPreferredNetworkType(aType);
  return request.then(null, () => { throw request.error });
}

/**
 * Query current preferred network type.
 *
 * Fulfill params:
 *   'wcdma/gsm', 'gsm', 'wcdma', 'wcdma/gsm-auto', 'cdma/evdo', 'cdma',
 *   'evdo', 'wcdma/gsm/cdma/evdo', 'lte/cdma/evdo', 'lte/wcdma/gsm',
 *   'lte/wcdma/gsm/cdma/evdo' or 'lte'.
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'.
 *
 * @return A deferred promise.
 */
 function getPreferredNetworkType() {
  let request = mobileConnection.getPreferredNetworkType();
  return request.then(() => request.result, () => { throw request.error });
}

/**
 * Configures call forward options.
 *
 * Fulfill params: (none)
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', 'InvalidParameter', or
 *   'GenericFailure'.
 *
 * @param aOptions
 *        A MozCallForwardingOptions.
 *
 * @return A deferred promise.
 */
 function setCallForwardingOption(aOptions) {
  let request = mobileConnection.setCallForwardingOption(aOptions);
  return request.then(null, () => { throw request.error });
}

/**
 * Configures call forward options.
 *
 * Fulfill params:
 *   An array of MozCallForwardingOptions.
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', 'InvalidParameter', or
 *   'GenericFailure'.
 *
 * @param aReason
 *        One of MozMobileConnection.CALL_FORWARD_REASON_* values.
 *
 * @return A deferred promise.
 */
 function getCallForwardingOption(aReason) {
  let request = mobileConnection.getCallForwardingOption(aReason);
  return request.then(() => request.result, () => { throw request.error });
}

/**
 * Set voice privacy preference.
 *
 * Fulfill params: (none)
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'.
 *
 * @param aEnabled
 *        Boolean indicates the preferred voice privacy mode.
 *
 * @return A deferred promise.
 */
 function setVoicePrivacyMode(aEnabled) {
  let request = mobileConnection.setVoicePrivacyMode(aEnabled);
  return request.then(null, () => { throw request.error });
}

/**
 * Query current voice privacy mode.
 *
 * Fulfill params:
 *   A boolean indicates the current voice privacy mode.
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'.
 *
 * @return A deferred promise.
 */
 function getVoicePrivacyMode() {
  let request = mobileConnection.getVoicePrivacyMode();
  return request.then(() => request.result, () => { throw request.error });
}

/**
 * Configures call barring options.
 *
 * Fulfill params: (none)
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', 'InvalidParameter' or
 *   'GenericFailure'.
 *
 * @return A deferred promise.
 */
 function setCallBarringOption(aOptions) {
  let request = mobileConnection.setCallBarringOption(aOptions);
  return request.then(null, () => { throw request.error });
}

/**
 * Queries current call barring status.
 *
 * Fulfill params:
 *   An object contains call barring status.
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', 'InvalidParameter' or
 *   'GenericFailure'.
 *
 * @return A deferred promise.
 */
 function getCallBarringOption(aOptions) {
  let request = mobileConnection.getCallBarringOption(aOptions);
  return request.then(() => request.result, () => { throw request.error });
}

/**
 * Change call barring facility password.
 *
 * Fulfill params: (none)
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'.
 *
 * @return A deferred promise.
 */
 function changeCallBarringPassword(aOptions) {
  let request = mobileConnection.changeCallBarringPassword(aOptions);
  return request.then(null, () => { throw request.error });
}

/**
 * Set data connection enabling state and wait for "datachange" event.
 *
 * Resolve if data connection state changed to the expected one.  Never reject.
 *
 * Fulfill params: (none)
 *
 * @param aEnabled
 *        A boolean state.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A deferred promise.
 */
function setDataEnabledAndWait(aEnabled, aServiceId) {
  let deferred = Promise.defer();

  let promises = [];
  promises.push(waitForManagerEvent("datachange", aServiceId));
  promises.push(setDataEnabled(aEnabled));
  Promise.all(promises).then(function keepWaiting() {
    let mobileConn = getMozMobileConnectionByServiceId(aServiceId);
    // To ignore some transient states, we only resolve that deferred promise
    // when the |connected| state equals to the expected one and never rejects.
    let connected = mobileConn.data.connected;
    if (connected == aEnabled) {
      deferred.resolve();
      return;
    }

    return waitForManagerEvent("datachange", aServiceId).then(keepWaiting);
  });

  return deferred.promise;
}

/**
 * Set radio enabling state.
 *
 * Resolve no matter the request succeeds or fails. Never reject.
 *
 * Fulfill params: (none)
 *
 * @param aEnabled
 *        A boolean state.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A deferred promise.
 */
function setRadioEnabled(aEnabled, aServiceId) {
  let mobileConn = getMozMobileConnectionByServiceId(aServiceId);
  let request = mobileConn.setRadioEnabled(aEnabled);
  return request.then(function onsuccess() {
      ok(true, "setRadioEnabled " + aEnabled + " on " + aServiceId + " success.");
    }, function onerror() {
      ok(false, "setRadioEnabled " + aEnabled + " on " + aServiceId + " " +
         request.error.name);
    });
}

/**
 * Set radio enabling state and wait for "radiostatechange" event.
 *
 * Resolve if radio state changed to the expected one. Never reject.
 *
 * Fulfill params: (none)
 *
 * @param aEnabled
 *        A boolean state.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A deferred promise.
 */
function setRadioEnabledAndWait(aEnabled, aServiceId) {
  let mobileConn = getMozMobileConnectionByServiceId(aServiceId);

  if (mobileConn.radioState === (aEnabled ? "enabled" : "disabled")) {
    return Promise.resolve();
  }

  let expectedSequence = aEnabled ? ["enabling", "enabled"] :
                                    ["disabling", "disabled"];

  let p1 = waitForManagerEvent("radiostatechange", aServiceId, function() {
    let mobileConn = getMozMobileConnectionByServiceId(aServiceId);
    let expectedRadioState = expectedSequence.shift();
    is(mobileConn.radioState, expectedRadioState, "Check radio state");
    return expectedSequence.length === 0;
  });

  let p2 = setRadioEnabled(aEnabled, aServiceId);

  return Promise.all([p1, p2]);
}

/**
 * Set CLIR (calling line id restriction).
 *
 * Fulfill params: (none)
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'
 *
 * @param aMode
 *        A short number.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A deferred promise.
 */
function setClir(aMode, aServiceId) {
  ok(true, "setClir(" + aMode + ", " + aServiceId + ")");
  let mobileConn = getMozMobileConnectionByServiceId(aServiceId);
  let request = mobileConn.setCallingLineIdRestriction(aMode);
  return request.then(null, () => { throw request.error });
}

/**
 * Get CLIR (calling line id restriction).
 *
 * Fulfill params:
 *   CLIR mode.
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'
 *
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A deferred promise.
 */
function getClir(aServiceId) {
  ok(true, "getClir(" + aServiceId + ")");
  let mobileConn = getMozMobileConnectionByServiceId(aServiceId);
  let request = mobileConn.getCallingLineIdRestriction();
  return request.then(() => request.result, () => { throw request.error });
}

/**
 * Set voice/data state and wait for state change.
 *
 * Fulfill params: (none)
 *
 * @param aWhich
 *        "voice" or "data".
 * @param aState
 *        "unregistered", "searching", "denied", "roaming", or "home".
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A deferred promise.
 */
function setEmulatorVoiceDataStateAndWait(aWhich, aState, aServiceId) {
  let promises = [];
  promises.push(waitForManagerEvent(aWhich + "change", aServiceId));

  let cmd = "gsm " + aWhich + " " + aState;
  promises.push(runEmulatorCmdSafe(cmd));
  return Promise.all(promises);
}

/**
 * Set voice and data roaming emulation and wait for state change.
 *
 * Fulfill params: (none)
 *
 * @param aRoaming
 *        A boolean state.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A deferred promise.
 */
function setEmulatorRoamingAndWait(aRoaming, aServiceId) {
  function doSetAndWait(aWhich, aRoaming, aServiceId) {
    let state = (aRoaming ? "roaming" : "home");
    return setEmulatorVoiceDataStateAndWait(aWhich, state, aServiceId)
      .then(() => {
        let mobileConn = getMozMobileConnectionByServiceId(aServiceId);
        is(mobileConn[aWhich].roaming, aRoaming,
                     aWhich + ".roaming")
      });
  }

  // Set voice registration state first and then data registration state.
  return doSetAndWait("voice", aRoaming, aServiceId)
    .then(() => doSetAndWait("data", aRoaming, aServiceId));
}

/**
 * Get GSM location emulation.
 *
 * Fulfill params:
 *   { lac: <lac>, cid: <cid> }
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function getEmulatorGsmLocation() {
  let cmd = "gsm location";
  return runEmulatorCmdSafe(cmd)
    .then(function(aResults) {
      // lac: <lac>
      // ci: <cid>
      // OK
      is(aResults[0].substring(0,3), "lac", "lac output");
      is(aResults[1].substring(0,2), "ci", "ci output");

      let lac = parseInt(aResults[0].substring(5));
      let cid = parseInt(aResults[1].substring(4));
      return { lac: lac, cid: cid };
    });
}

/**
 * Set GSM location emulation.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @param aLac
 * @param aCid
 *
 * @return A deferred promise.
 */
function setEmulatorGsmLocation(aLac, aCid) {
  let cmd = "gsm location " + aLac + " " + aCid;
  return runEmulatorCmdSafe(cmd);
}

/**
 * Set GSM location and wait for voice and/or data state change.
 *
 * Fulfill params: (none)
 *
 * @param aLac
 * @param aCid
 * @param aWaitVoice [optional]
 *        A boolean value.  Default true.
 * @param aWaitData [optional]
 *        A boolean value.  Default false.
 *
 * @return A deferred promise.
 */
function setEmulatorGsmLocationAndWait(aLac, aCid,
                                       aWaitVoice = true, aWaitData = false) {
  let promises = [];
  if (aWaitVoice) {
    promises.push(waitForManagerEvent("voicechange"));
  }
  if (aWaitData) {
    promises.push(waitForManagerEvent("datachange"));
  }
  promises.push(setEmulatorGsmLocation(aLac, aCid));
  return Promise.all(promises);
}

/**
 * Get emulator operators info.
 *
 * Fulfill params:
 *   An array of { longName: <string>, shortName: <string>, mccMnc: <string> }.
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function getEmulatorOperatorNames() {
  let cmd = "operator dumpall";
  return runEmulatorCmdSafe(cmd)
    .then(function(aResults) {
      let operators = [];

      for (let i = 0; i < aResults.length - 1; i++) {
        let names = aResults[i].split(',');
        operators.push({
          longName: names[0],
          shortName: names[1],
          mccMnc: names[2],
        });
      }

      ok(true, "emulator operators list: " + JSON.stringify(operators));
      return operators;
    });
}

/**
 * Set emulator operators info.
 *
 * Fulfill params: (none)
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @param aOperator
 *        "home" or "roaming".
 * @param aLongName
 *        A string.
 * @param aShortName
 *        A string.
 * @param aMcc [optional]
 *        A string.
 * @param aMnc [optional]
 *        A string.
 *
 * @return A deferred promise.
 */
function setEmulatorOperatorNames(aOperator, aLongName, aShortName, aMcc, aMnc) {
  const EMULATOR_OPERATORS = [ "home", "roaming" ];

  let index = EMULATOR_OPERATORS.indexOf(aOperator);
  if (index < 0) {
    throw "invalid operator";
  }

  let cmd = "operator set " + index + " " + aLongName + "," + aShortName;
  if (aMcc && aMnc) {
    cmd = cmd + "," + aMcc + aMnc;
  }
  return runEmulatorCmdSafe(cmd)
    .then(function(aResults) {
      let exp = "^" + aLongName + "," + aShortName + ",";
      if (aMcc && aMnc) {
        cmd = cmd + aMcc + aMnc;
      }

      let re = new RegExp(exp);
      ok(aResults[index].match(new RegExp(exp)),
         "Long/short name and/or mcc/mnc should be changed.");
    });
}

/**
 * Set emulator operators info and wait for voice and/or data state change.
 *
 * Fulfill params: (none)
 *
 * @param aOperator
 *        "home" or "roaming".
 * @param aLongName
 *        A string.
 * @param aShortName
 *        A string.
 * @param aMcc [optional]
 *        A string.
 * @param aMnc [optional]
 *        A string.
 * @param aWaitVoice [optional]
 *        A boolean value.  Default true.
 * @param aWaitData [optional]
 *        A boolean value.  Default false.
 *
 * @return A deferred promise.
 */
function setEmulatorOperatorNamesAndWait(aOperator, aLongName, aShortName,
                                         aMcc, aMnc,
                                         aWaitVoice = true, aWaitData = false) {
  let promises = [];
  if (aWaitVoice) {
    promises.push(waitForManagerEvent("voicechange"));
  }
  if (aWaitData) {
    promises.push(waitForManagerEvent("datachange"));
  }
  promises.push(setEmulatorOperatorNames(aOperator, aLongName, aShortName,
                                         aMcc, aMnc));
  return Promise.all(promises);
}

/**
 * Set GSM signal strength.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @param aRssi
 *
 * @return A deferred promise.
 */
function setEmulatorGsmSignalStrength(aRssi) {
  let cmd = "gsm signal " + aRssi;
  return runEmulatorCmdSafe(cmd);
}

/**
 * Set emulator GSM signal strength and wait for voice and/or data state change.
 *
 * Fulfill params: (none)
 *
 * @param aRssi
 * @param aWaitVoice [optional]
 *        A boolean value.  Default true.
 * @param aWaitData [optional]
 *        A boolean value.  Default false.
 *
 * @return A deferred promise.
 */
function setEmulatorGsmSignalStrengthAndWait(aRssi,
                                             aWaitVoice = true,
                                             aWaitData = false) {
  let promises = [];
  if (aWaitVoice) {
    promises.push(waitForManagerEvent("voicechange"));
  }
  if (aWaitData) {
    promises.push(waitForManagerEvent("datachange"));
  }
  promises.push(setEmulatorGsmSignalStrength(aRssi));
  return Promise.all(promises);
}

/**
 * Set LTE signal strength.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @param aRxlev
 * @param aRsrp
 * @param aRssnr
 *
 * @return A deferred promise.
 */
function setEmulatorLteSignalStrength(aRxlev, aRsrp, aRssnr) {
  let cmd = "gsm lte_signal " + aRxlev + " " + aRsrp + " " + aRssnr;
  return runEmulatorCmdSafe(cmd);
}

/**
 * Set emulator LTE signal strength and wait for voice and/or data state change.
 *
 * Fulfill params: (none)
 *
 * @param aRxlev
 * @param aRsrp
 * @param aRssnr
 * @param aWaitVoice [optional]
 *        A boolean value.  Default true.
 * @param aWaitData [optional]
 *        A boolean value.  Default false.
 *
 * @return A deferred promise.
 */
function setEmulatorLteSignalStrengthAndWait(aRxlev, aRsrp, aRssnr,
                                             aWaitVoice = true,
                                             aWaitData = false) {
  let promises = [];
  if (aWaitVoice) {
    promises.push(waitForManagerEvent("voicechange"));
  }
  if (aWaitData) {
    promises.push(waitForManagerEvent("datachange"));
  }
  promises.push(setEmulatorLteSignalStrength(aRxlev, aRsrp, aRssnr));
  return Promise.all(promises);
}

let _networkManager;

/**
 * Get internal NetworkManager service.
 */
function getNetworkManager() {
  if (!_networkManager) {
    _networkManager = Cc["@mozilla.org/network/manager;1"]
                    .getService(Ci.nsINetworkManager);
    ok(_networkManager, "NetworkManager");
  }

  return _networkManager;
}

let _numOfRadioInterfaces;

/*
 * Get number of radio interfaces. Default is 1 if preference is not set.
 */
function getNumOfRadioInterfaces() {
  if (!_numOfRadioInterfaces) {
    try {
      _numOfRadioInterfaces = SpecialPowers.getIntPref("ril.numRadioInterfaces");
    } catch (ex) {
      _numOfRadioInterfaces = 1;  // Pref not set.
    }
  }

  return _numOfRadioInterfaces;
}

/**
 * Wait for pending emulator transactions and call |finish()|.
 */
function cleanUp() {
  // Use ok here so that we have at least one test run.
  ok(true, ":: CLEANING UP ::");

  waitFor(finish, function() {
    return _pendingEmulatorCmdCount === 0 &&
           _pendingEmulatorShellCmdCount === 0;
  });
}

/**
 * Basic test routine helper for mobile connection tests.
 *
 * This helper does nothing but clean-ups.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 */
function startTestBase(aTestCaseMain) {
  // Turn on debugging pref.
  let debugPref = SpecialPowers.getBoolPref(PREF_KEY_RIL_DEBUGGING_ENABLED);
  SpecialPowers.setBoolPref(PREF_KEY_RIL_DEBUGGING_ENABLED, true);

  return Promise.resolve()
    .then(aTestCaseMain)
    .catch((aError) => {
      ok(false, "promise rejects during test: " + aError);
    })
    .then(() => {
      // Restore debugging pref.
      SpecialPowers.setBoolPref(PREF_KEY_RIL_DEBUGGING_ENABLED, debugPref);
      cleanUp();
    })
}

/**
 * Common test routine helper for mobile connection tests.
 *
 * This function ensures global |mobileConnection| variable is available during
 * the process and performs clean-ups as well.
 *
 * @param aTestCaseMain
 *        A function that takes one parameter -- mobileConnection.
 * @param aAdditonalPermissions [optional]
 *        An array of permission strings other than "mobileconnection" to be
 *        pushed. Default: empty string.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: 0.
 */
function startTestCommon(aTestCaseMain, aAdditionalPermissions, aServiceId) {
  startTestBase(function() {
    return ensureMobileConnection(aAdditionalPermissions, aServiceId)
      .then(aTestCaseMain);
  });
}

/**
 * Common test routine helper for multi-sim mobile connection tests. The test
 * ends immediately if the device tested is not multi-sim.
 *
 * This function ensures global |mobileConnection| variable is available during
 * the process and performs clean-ups as well.
 *
 * @param aTestCaseMain
 *        A function that takes one parameter -- mobileConnection.
 * @param aAdditonalPermissions [optional]
 *        An array of permission strings other than "mobileconnection" to be
 *        pushed. Default: empty string.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: 0.
 */
function startDSDSTestCommon(aTestCaseMain, aAdditionalPermissions, aServiceId) {
    if (getNumOfRadioInterfaces() > 1) {
      startTestBase(function() {
        return ensureMobileConnection(aAdditionalPermissions, aServiceId)
          .then(aTestCaseMain);
      });
    } else {
      log("Skipping DSDS tests on single SIM device.")
      ok(true);  // We should run at least one test.
      cleanUp();
    }
}
