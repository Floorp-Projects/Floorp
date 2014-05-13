/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

const SETTINGS_KEY_DATA_ENABLED = "ril.data.enabled";
const SETTINGS_KEY_DATA_ROAMING_ENABLED = "ril.data.roaming_enabled";
const SETTINGS_KEY_DATA_APN_SETTINGS = "ril.data.apnSettings";

let Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

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
  let deferred = Promise.defer();

  ++_pendingEmulatorCmdCount;
  runEmulatorCmd(aCommand, function(aResult) {
    --_pendingEmulatorCmdCount;

    ok(true, "Emulator response: " + JSON.stringify(aResult));
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

/**
 * Wrap DOMRequest onsuccess/onerror events to Promise resolve/reject.
 *
 * Fulfill params: A DOMEvent.
 * Reject params: A DOMEvent.
 *
 * @param aRequest
 *        A DOMRequest instance.
 *
 * @return A deferred promise.
 */
function wrapDomRequestAsPromise(aRequest) {
  let deferred = Promise.defer();

  ok(aRequest instanceof DOMRequest,
     "aRequest is instanceof " + aRequest.constructor);

  aRequest.addEventListener("success", function(aEvent) {
    deferred.resolve(aEvent);
  });
  aRequest.addEventListener("error", function(aEvent) {
    deferred.reject(aEvent);
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
  return wrapDomRequestAsPromise(request)
    .then(function resolve(aEvent) {
      ok(true, "getSettings(" + aKey + ") - success");
      return aEvent.target.result[aKey];
    }, function reject(aEvent) {
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
  let request =
    workingFrame.contentWindow.navigator.mozSettings.createLock().set(aSettings);
  return wrapDomRequestAsPromise(request)
    .then(function resolve() {
      ok(true, "setSettings(" + JSON.stringify(aSettings) + ")");
    }, function reject() {
      ok(aAllowError, "setSettings(" + JSON.stringify(aSettings) + ")");
    });
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
 *
 * @return A deferred promise.
 */
function waitForManagerEvent(aEventName, aServiceId) {
  let deferred = Promise.defer();

  let mobileConn = getMozMobileConnectionByServiceId(aServiceId);

  mobileConn.addEventListener(aEventName, function onevent(aEvent) {
    mobileConn.removeEventListener(aEventName, onevent);

    ok(true, "MobileConnection event '" + aEventName + "' got.");
    deferred.resolve(aEvent);
  });

  return deferred.promise;
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
  return wrapDomRequestAsPromise(request)
    .then(() => request.result);
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
  return wrapDomRequestAsPromise(request)
    .then(null, () => { throw request.error });
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
  return wrapDomRequestAsPromise(request)
    .then(null, () => { throw request.error });
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
 * Send a MMI message.
 *
 * Fulfill params: An object contains MMI result.
 * Reject params: A DOMMMIError.
 *
 * @param aMmi
 *        A MMI string.
 *
 * @return A deferred promise.
 */
function sendMMI(aMmi) {
  let request = mobileConnection.sendMMI(aMmi);
  return wrapDomRequestAsPromise(request)
    .then(() => request.result, () => { throw request.error });
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
  return wrapDomRequestAsPromise(request)
    .then(null, () => { throw request.error });
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
  return wrapDomRequestAsPromise(request)
    .then(null, () => { throw request.error });
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
  return wrapDomRequestAsPromise(request)
    .then(() => request.result, () => { throw request.error });
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
  return wrapDomRequestAsPromise(request)
    .then(null, () => { throw request.error });
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
  return wrapDomRequestAsPromise(request)
    .then(() => request.result, () => { throw request.error });
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
  return wrapDomRequestAsPromise(request)
    .then(null, () => { throw request.error });
}

/**
 * Query current voice privacy mode.
 *
 * Fulfill params:
     A boolean indicates the current voice privacy mode.
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'.
 *
 * @return A deferred promise.
 */
 function getVoicePrivacyMode() {
  let request = mobileConnection.getVoicePrivacyMode();
  return wrapDomRequestAsPromise(request)
    .then(() => request.result, () => { throw request.error });
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
  return wrapDomRequestAsPromise(request)
    .then(function onsuccess() {
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
  let deferred = Promise.defer();

  let promises = [];
  promises.push(waitForManagerEvent("radiostatechange", aServiceId));
  promises.push(setRadioEnabled(aEnabled, aServiceId));
  Promise.all(promises).then(function keepWaiting() {
    let mobileConn = getMozMobileConnectionByServiceId(aServiceId);
    // To ignore some transient states, we only resolve that deferred promise
    // when |radioState| equals to the expected one and never rejects.
    let state = mobileConn.radioState;
    aEnabled = aEnabled ? "enabled" : "disabled";
    if (state == aEnabled) {
      deferred.resolve();
      return;
    }

    return waitForManagerEvent("radiostatechange", aServiceId).then(keepWaiting);
  });

  return deferred.promise;
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
  return wrapDomRequestAsPromise(request)
    .then(null, () => { throw request.error });
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
  return wrapDomRequestAsPromise(request)
    .then(() => request.result, () => { throw request.error });
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
 * Flush permission settings and call |finish()|.
 */
function cleanUp() {
  waitFor(function() {
    SpecialPowers.flushPermissions(function() {
      // Use ok here so that we have at least one test run.
      ok(true, "permissions flushed");

      finish();
    });
  }, function() {
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
  Promise.resolve()
    .then(aTestCaseMain)
    .then(cleanUp, function() {
      ok(false, 'promise rejects during test.');
      cleanUp();
    });
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
