/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_CONTEXT = "chrome";

const SETTINGS_KEY_DATA_ENABLED = "ril.data.enabled";
const SETTINGS_KEY_DATA_APN_SETTINGS  = "ril.data.apnSettings";

const TOPIC_CONNECTION_STATE_CHANGED = "network-connection-state-changed";
const TOPIC_NETWORK_ACTIVE_CHANGED = "network-active-changed";

let Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

let ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
ok(ril, "ril.constructor is " + ril.constructor);

let radioInterface = ril.getRadioInterface(0);
ok(radioInterface, "radioInterface.constructor is " + radioInterface.constrctor);

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
  return wrapDomRequestAsPromise(request)
    .then(function resolve(aEvent) {
      log("getSettings(" + aKey + ") - success");
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
  let request = window.navigator.mozSettings.createLock().set(settings);
  return wrapDomRequestAsPromise(request)
    .then(function resolve() {
      log("setSettings(" + JSON.stringify(settings) + ") - success");
    }, function reject() {
      ok(aAllowError, "setSettings(" + JSON.stringify(settings) + ") - error");
    });
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
  }, aTopic, false);

  return deferred.promise;
}

let mobileTypeMapping = {
  "default": Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
  "mms": Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS,
  "supl": Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL,
  "ims": Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_IMS,
  "dun": Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_DUN
};

/**
 * Set the default data connection enabling state, wait for
 * "network-connection-state-changed" event and verify state.
 *
 * Fulfill params: (none)
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
      ok(aSubject instanceof Ci.nsIRilNetworkInterface,
         "subject should be an instance of nsIRILNetworkInterface");
      is(aSubject.type, Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
         "subject.type should be " + Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE);
      is(aSubject.state,
         aEnabled ? Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED
                  : Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED,
         "subject.state should be " + aEnabled ? "CONNECTED" : "DISCONNECTED");
    }));
  promises.push(setSettings(SETTINGS_KEY_DATA_ENABLED, aEnabled));

  return Promise.all(promises);
}

/**
 * Setup a certain type of data connection, wait for
 * "network-connection-state-changed" event and verify state.
 *
 * Fulfill params: (none)
 *
 * @param aType
 *        The string of the type of data connection to setup.
 *
 * @return A deferred promise.
 */
function setupDataCallAndWait(aType) {
  log("setupDataCallAndWait: " + aType);

  let promises = [];
  promises.push(waitForObserverEvent(TOPIC_CONNECTION_STATE_CHANGED)
    .then(function(aSubject) {
      let networkType = mobileTypeMapping[aType];
      ok(aSubject instanceof Ci.nsIRilNetworkInterface,
         "subject should be an instance of nsIRILNetworkInterface");
      is(aSubject.type, networkType,
         "subject.type should be " + networkType);
      is(aSubject.state, Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED,
         "subject.state should be CONNECTED");
    }));
  promises.push(radioInterface.setupDataCallByType(aType));

  return Promise.all(promises);
}

/**
 * Deactivate a certain type of data connection, wait for
 * "network-connection-state-changed" event and verify state.
 *
 * Fulfill params: (none)
 *
 * @param aType
 *        The string of the type of data connection to deactivate.
 *
 * @return A deferred promise.
 */
function deactivateDataCallAndWait(aType) {
  log("deactivateDataCallAndWait: " + aType);

  let promises = [];
  promises.push(waitForObserverEvent(TOPIC_CONNECTION_STATE_CHANGED)
    .then(function(aSubject) {
      let networkType = mobileTypeMapping[aType];
      ok(aSubject instanceof Ci.nsIRilNetworkInterface,
         "subject should be an instance of nsIRILNetworkInterface");
      is(aSubject.type, networkType,
         "subject.type should be " + networkType);
      is(aSubject.state, Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED,
         "subject.state should be DISCONNECTED");
    }));
  promises.push(radioInterface.deactivateDataCallByType(aType));

  return Promise.all(promises);
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
    .then(finish, function() {
      ok(false, 'promise rejects during test.');
      finish();
    });
}
