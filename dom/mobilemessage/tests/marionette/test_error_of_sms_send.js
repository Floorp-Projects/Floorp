/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const kPrefRilRadioDisabled  = "ril.radio.disabled";

let connection;
function ensureMobileConnection() {
  let deferred = Promise.defer();

  let permissions = [{
    "type": "mobileconnection",
    "allow": true,
    "context": document,
  }];
  SpecialPowers.pushPermissions(permissions, function() {
    ok(true, "permissions pushed: " + JSON.stringify(permissions));

    connection = window.navigator.mozMobileConnections[0];
    if (connection) {
      log("navigator.mozMobileConnections[0] is instance of " + connection.constructor);
    } else {
      log("navigator.mozMobileConnections[0] is undefined.");
    }

    if (connection instanceof MozMobileConnection) {
      deferred.resolve(connection);
    } else {
      deferred.reject();
    }
  });

  return deferred.promise;
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

function setRadioEnabled(enabled) {
  log("setRadioEnabled to " + enabled);

  let deferred = Promise.defer();

  let finalState = (enabled) ? "enabled" : "disabled";
  connection.onradiostatechange = function() {
    let state = connection.radioState;
    log("Received 'radiostatechange' event, radioState: " + state);

    if (state == finalState) {
      deferred.resolve();
      connection.onradiostatechange = null;
    }
  };

  let req = connection.setRadioEnabled(enabled);

  req.onsuccess = function() {
    log("setRadioEnabled success");
  };

  req.onerror = function() {
    ok(false, "setRadioEnabled should not fail");
    deferred.reject();
  };

  return deferred.promise;
}

function testSendFailed(aCause) {
  log("testSendFailed, aCause: " + aCause);

  let testReceiver = "+0987654321";
  let testMessage = "quick fox jump over the lazy dog";

  return sendSmsWithFailure(testReceiver, testMessage)
    .then((result) => {
      is(result.error.name, aCause, "Checking failure cause.");

      let domMessage = result.error.data;
      is(domMessage.id, result.message.id, "Checking message id.");
      is(domMessage.receiver, testReceiver, "Checking receiver address.");
      is(domMessage.body, testMessage, "Checking message body.");
      is(domMessage.delivery, "error", "Checking delivery.");
      is(domMessage.deliveryStatus, "error", "Checking deliveryStatus.");
    });
}

startTestCommon(function testCaseMain() {
  return ensureMobileConnection()
    .then(() => waitRadioState("enabled"))
    .then(() => setRadioEnabled(false))
    .then(() => testSendFailed("RadioDisabledError"))
    .then(() => setRadioEnabled(true));
});
