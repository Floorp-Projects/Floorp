/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const DATA_KEY  = "ril.data.enabled";
const APN_KEY   = "ril.data.apnSettings";

let Promise = SpecialPowers.Cu.import("resource://gre/modules/Promise.jsm").Promise;

SpecialPowers.setBoolPref("dom.mozSettings.enabled", true);
SpecialPowers.addPermission("mobileconnection", true, document);
SpecialPowers.addPermission("settings-read", true, document);
SpecialPowers.addPermission("settings-write", true, document);

let settings = window.navigator.mozSettings;
let connection = window.navigator.mozMobileConnections[0];
ok(connection instanceof MozMobileConnection,
   "connection is instanceof " + connection.constructor);

function setSetting(key, value) {
  let deferred = Promise.defer();

  let setLock = settings.createLock();
  let obj = {};
  obj[key] = value;

  let setReq = setLock.set(obj);
  setReq.addEventListener("success", function onSetSuccess() {
    ok(true, "set '" + key + "' to " + obj[key]);
    deferred.resolve();
  });
  setReq.addEventListener("error", function onSetError() {
    ok(false, "cannot set '" + key + "'");
    deferred.reject();
  });

  return deferred.promise;
}

function setEmulatorAPN() {
  let apn =
    [
      [
        {"carrier":"T-Mobile US",
         "apn":"epc.tmobile.com",
         "mmsc":"http://mms.msg.eng.t-mobile.com/mms/wapenc",
         "types":["default","supl","mms"]}
      ]
    ];
  return setSetting(APN_KEY, apn);
}

function enableData() {
  log("Turn data on.");

  let deferred = Promise.defer();

  connection.addEventListener("datachange", function ondatachange() {
    if (connection.data.connected === true) {
      connection.removeEventListener("datachange", ondatachange);
      log("mobileConnection.data.connected is now '"
          + connection.data.connected + "'.");
      deferred.resolve();
    }
  });

  setEmulatorAPN()
    .then(() => setSetting(DATA_KEY, true));

  return deferred.promise;
}

function receivedPending(received, pending, nextAction) {
  let index = pending.indexOf(received);
  if (index != -1) {
    pending.splice(index, 1);
  }
  if (pending.length === 0) {
    nextAction();
  }
}

function waitRadioState(state) {
  let deferred = Promise.defer();

  waitFor(function() {
    deferred.resolve();
  }, function() {
    return connection.radioState == state;
  });

  return deferred.promise;
}

function setRadioEnabled(enabled, transientState, finalState) {
  log("setRadioEnabled to " + enabled);

  let deferred = Promise.defer();
  let done = function() {
    deferred.resolve();
  };

  let pending = ["onradiostatechange", "onsuccess"];

  let receivedTransient = false;
  connection.onradiostatechange = function() {
    let state = connection.radioState;
    log("Received 'radiostatechange' event, radioState: " + state);

    if (state == transientState) {
      receivedTransient = true;
    } else if (state == finalState) {
      ok(receivedTransient);
      receivedPending("onradiostatechange", pending, done);
    }
  };

  let req = connection.setRadioEnabled(enabled);

  req.onsuccess = function() {
    log("setRadioEnabled success");
    receivedPending("onsuccess", pending, done);
  };

  req.onerror = function() {
    ok(false, "setRadioEnabled should not fail");
    deferred.reject();
  };

  return deferred.promise;
}

function testSwitchRadio() {
  log("= testSwitchRadio =");
  return waitRadioState("enabled")
    .then(setRadioEnabled.bind(null, false, "disabling", "disabled"))
    .then(setRadioEnabled.bind(null, true, "enabling", "enabled"));
}

function testDisableRadioWhenDataConnected() {
  log("= testDisableRadioWhenDataConnected =");
  return waitRadioState("enabled")
    .then(enableData)
    .then(setRadioEnabled.bind(null, false, "disabling", "disabled"))
    .then(() => {
      // Data should be disconnected.
      is(connection.data.connected, false);
    })
    .then(setRadioEnabled.bind(null, true, "enabling", "enabled"));
}

function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);
  SpecialPowers.removePermission("settings-write", document);
  SpecialPowers.removePermission("settings-read", document);
  SpecialPowers.clearUserPref("dom.mozSettings.enabled");
  finish();
}

testSwitchRadio()
  .then(testDisableRadioWhenDataConnected)
  .then(null, () => {
    ok(false, "promise reject somewhere");
  })
  .then(cleanUp);
