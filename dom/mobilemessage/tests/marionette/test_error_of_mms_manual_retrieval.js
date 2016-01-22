/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

// We apply "chrome" context to be more flexible to
// specify the content of M-Notification.ind such as iccId
// for different kinds of testing.
MARIONETTE_CONTEXT = "chrome";

Cu.import("resource://gre/modules/Promise.jsm");

var MMS = {};
Cu.import("resource://gre/modules/MmsPduHelper.jsm", MMS);

var gMobileMessageDatabaseService =
  Cc["@mozilla.org/mobilemessage/gonkmobilemessagedatabaseservice;1"]
    .getService(Ci.nsIGonkMobileMessageDatabaseService);

var gUuidGenerator =
  Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

var gMmsService =
  Cc["@mozilla.org/mms/gonkmmsservice;1"].getService(Ci.nsIMmsService);

var gMobileConnectionService =
  Cc["@mozilla.org/mobileconnection/mobileconnectionservice;1"]
    .getService(Ci.nsIMobileConnectionService);

var gIccService =
  Cc["@mozilla.org/icc/gonkiccservice;1"].getService(Ci.nsIIccService);

function saveMmsNotification() {
  log("saveMmsNotification()");
  let tid = gUuidGenerator.generateUUID().toString();
  tid = tid.substr(1, tid.length - 2); // strip parentheses {}.

  let headers = {};
  headers["x-mms-message-type"] = MMS.MMS_PDU_TYPE_NOTIFICATION_IND;
  headers["x-mms-transaction-id"] = tid;
  headers["x-mms-mms-version"] = MMS.MMS_VERSION;
  headers["x-mms-message-class"] = "personal";
  headers["x-mms-message-size"] = 255;
  headers["x-mms-expiry"] = 24 * 60 * 60;
  headers["x-mms-content-location"] = "http://192.168.0.1/mms.cgi" + "?tid=" + tid;
  let notification = {
    headers: headers,
    type: "mms",
    delivery: "not-downloaded",
    deliveryStatus: "manual",
    timestamp: Date.now(),
    receivers: [],
    phoneNumber: "+0987654321",
    iccId: "01234567899876543210"
  };

  let deferred = Promise.defer();

  gMobileMessageDatabaseService
    .saveReceivedMessage(notification, function(aRv, aDomMessage) {
      log("saveReceivedMessage(): " + aRv);
      if (Components.isSuccessCode(aRv)) {
        deferred.resolve(aDomMessage.QueryInterface(Ci.nsIMmsMessage));
      } else {
        deferred.reject();
      }
    });

  return deferred.promise;
}

function deleteMessagesById(aIds) {
  log("deleteMessagesById()");
  let deferred = Promise.defer();
  let request = {
    notifyDeleteMessageFailed: function(aRv) {
      log("notifyDeleteMessageFailed()");
      deferred.reject();
    },
    notifyMessageDeleted: function(aDeleted, aLength) {
      log("notifyMessageDeleted()");
      deferred.resolve();
    },
  };
  gMobileMessageDatabaseService
    .deleteMessage(aIds, aIds.length, request);

  return deferred.promise;
}

function verifyErrorCause(aResponse, aCause) {
  is(aResponse, aCause, "Test error cause of retrieval.");

  return deleteMessagesById([aResponse.id]);
}

function retrieveMmsWithFailure(aId) {
  log("retrieveMmsWithFailure()");
  let deferred = Promise.defer();
  let request = {
    notifyGetMessageFailed: function(aRv) {
      log("notifyGetMessageFailed()");
      deferred.resolve(aRv);
    }
  };
  gMmsService.retrieve(aId, request);

  return deferred.promise;
}

function testRetrieve(aCause) {
  log("testRetrieve: aCause = " + aCause);
  return Promise.resolve()
    .then(saveMmsNotification)
    .then((message) => retrieveMmsWithFailure(message.id))
    .then((response) => verifyErrorCause(response, aCause));
}

function setRadioEnabled(aConnection, aEnabled) {
  let deferred = Promise.defer();
  let finalState = (aEnabled) ?
    Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED :
    Ci.nsIMobileConnection.MOBILE_RADIO_STATE_DISABLED;

  if (aConnection.radioState == finalState) {
    return deferred.resolve(aConnection);
  }
  let listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileConnectionListener]),
    notifyVoiceChanged: function() {},
    notifyDataChanged: function() {},
    notifyDataError: function(message) {},
    notifyCFStateChanged: function(action, reason, number, timeSeconds, serviceClass) {},
    notifyEmergencyCbModeChanged: function(active, timeoutMs) {},
    notifyOtaStatusChanged: function(status) {},
    notifyRadioStateChanged: function() {
      log("setRadioEnabled state changed to " + aConnection.radioState);
      if (aConnection.radioState == finalState) {
        aConnection.unregisterListener(listener);
        deferred.resolve(aConnection);
      }
    },
    notifyClirModeChanged: function(mode) {},
    notifyLastKnownNetworkChanged: function() {},
    notifyLastKnownHomeNetworkChanged: function() {},
    notifyNetworkSelectionModeChanged: function() {},
    notifyDeviceIdentitiesChanged: function() {}
  };
  let callback = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileConnectionCallback]),
    notifySuccess() {},
    notifySuccessWithBoolean(result) {},
    notifyGetNetworksSuccess(count, networks) {},
    notifyGetCallForwardingSuccess(count, results) {},
    notifyGetCallBarringSuccess(program, enabled, serviceClass) {},
    notifyGetCallWaitingSuccess(serviceClass) {},
    notifyGetClirStatusSuccess(n, m) {},
    notifyGetPreferredNetworkTypeSuccess(type) {},
    notifyGetRoamingPreferenceSuccess(mode) {},
    notifyError(name) {
      log("setRadioEnabled reject");
      aConnection.unregisterListener(listener);
      deferred.reject();
    }
  };
  aConnection.registerListener(listener);
  aConnection.setRadioEnabled(aEnabled, callback);
  return deferred.promise;
}

function setAllRadioEnabled(aEnabled) {
  log("setAllRadioEnabled connection number = " +
    gMobileConnectionService.numItems);

  let promises = [];
  for (let i = 0; i < gMobileConnectionService.numItems; ++i) {
    promises.push(setRadioEnabled(
      gMobileConnectionService.getItemByServiceId(i), aEnabled));
  }
  return Promise.all(promises);
}

function waitIccReady(aIcc) {
  let deferred = Promise.defer();
  if (aIcc.cardState == Ci.nsIIcc.CARD_STATE_READY) {
    return deferred.resolve(aIcc);
  }
  let listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIIccListener]),
    notifyStkCommand(aStkProactiveCmd) {},
    notifyStkSessionEnd() {},
    notifyCardStateChanged() {
      if (aIcc.cardState == Ci.nsIIcc.CARD_STATE_READY) {
        aIcc.unregisterListener(listener);
        deferred.resolve(aIcc);
      }
    },
    notifyIccInfoChanged() {}
  };
  aIcc.registerListener(listener);
  return deferred.promise;
}

function waitAllIccReady() {
  let promises = [];
  for (let i = 0; i < gMobileConnectionService.numItems; ++i) {
    let icc = gIccService.getIccByServiceId(i);
    promises.push(waitIccReady(icc));
  }
  return Promise.all(promises);
}

setAllRadioEnabled(false)
  .then(() => testRetrieve(Ci.nsIMobileMessageCallback.RADIO_DISABLED_ERROR))
  .then(() => setAllRadioEnabled(true))
  .then(() => waitAllIccReady())
  .then(() => testRetrieve(Ci.nsIMobileMessageCallback.SIM_NOT_MATCHED_ERROR))
  .then(finish);
