/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_CONTEXT = "chrome";

let Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

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
