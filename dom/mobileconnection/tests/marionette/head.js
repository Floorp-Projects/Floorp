/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

const SETTINGS_KEY_DATA_ENABLED = "ril.data.enabled";
const SETTINGS_KEY_DATA_ROAMING_ENABLED = "ril.data.roaming_enabled";
const SETTINGS_KEY_DATA_APN_SETTINGS = "ril.data.apnSettings";

let Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

let _pendingEmulatorCmdCount = 0;

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
    if (Array.isArray(aResult) && aResult[0] === "OK") {
      deferred.resolve(aResult);
    } else {
      deferred.reject(aResult);
    }
  });

  return deferred.promise;
}

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
  let deferred = Promise.defer();

  let request = navigator.mozSettings.createLock().get(aKey);
  request.addEventListener("success", function(aEvent) {
    ok(true, "getSettings(" + aKey + ") - success");
    deferred.resolve(aEvent.target.result[aKey]);
  });
  request.addEventListener("error", function() {
    ok(aAllowError, "getSettings(" + aKey + ") - error");
    deferred.reject();
  });

  return deferred.promise;
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
  let deferred = Promise.defer();

  let request = navigator.mozSettings.createLock().set(aSettings);
  request.addEventListener("success", function() {
    ok(true, "setSettings(" + JSON.stringify(aSettings) + ")");
    deferred.resolve();
  });
  request.addEventListener("error", function() {
    ok(aAllowError, "setSettings(" + JSON.stringify(aSettings) + ")");
    deferred.reject();
  });

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
    let ifr = document.createElement("iframe");
    ifr.addEventListener("load", function load() {
      ifr.removeEventListener("load", load);

      mobileConnection =
        ifr.contentWindow.navigator.mozMobileConnections[aServiceId];

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

    document.body.appendChild(ifr);
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
 *
 * @return A deferred promise.
 */
function waitForManagerEvent(aEventName) {
  let deferred = Promise.defer();

  mobileConnection.addEventListener(aEventName, function onevent(aEvent) {
    mobileConnection.removeEventListener(aEventName, onevent);

    ok(true, "MobileConnection event '" + aEventName + "' got.");
    deferred.resolve(aEvent);
  });

  return deferred.promise;
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
 *
 * @return A deferred promise.
 */
function setDataEnabledAndWait(aEnabled) {
  let deferred = Promise.defer();

  let promises = [];
  promises.push(waitForManagerEvent("datachange"));
  promises.push(setDataEnabled(aEnabled));
  Promise.all(promises).then(function keepWaiting() {
    // To ignore some transient states, we only resolve that deferred promise
    // when the |connected| state equals to the expected one and never rejects.
    let connected = mobileConnection.data.connected;
    if (connected == aEnabled) {
      deferred.resolve();
      return;
    }

    return waitForManagerEvent("datachange").then(keepWaiting);
  });

  return deferred.promise;
}

/**
 * Set voice/data roaming emulation and wait for state change.
 *
 * Fulfill params: (none)
 *
 * @param aRoaming
 *        A boolean state.
 *
 * @return A deferred promise.
 */
function setEmulatorRoamingAndWait(aRoaming) {
  function doSetAndWait(aWhich, aRoaming) {
    let promises = [];
    promises.push(waitForManagerEvent(aWhich + "change"));

    let cmd = "gsm " + aWhich + " " + (aRoaming ? "roaming" : "home");
    promises.push(runEmulatorCmdSafe(cmd));
    return Promise.all(promises)
      .then(() => is(mobileConnection[aWhich].roaming, aRoaming,
                     aWhich + ".roaming"));
  }

  // Set voice registration state first and then data registration state.
  return doSetAndWait("voice", aRoaming)
    .then(() => doSetAndWait("data", aRoaming));
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
    return _pendingEmulatorCmdCount === 0;
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
