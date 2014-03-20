/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
// We apply "chrome" context to be more flexible to
// specify the content of M-Notification.ind such as iccId
// for different kinds of testing.
MARIONETTE_CONTEXT = "chrome";

Cu.import("resource://gre/modules/Promise.jsm");

let MMS = {};
Cu.import("resource://gre/modules/MmsPduHelper.jsm", MMS);

let gMobileMessageDatabaseService =
  Cc["@mozilla.org/mobilemessage/rilmobilemessagedatabaseservice;1"]
    .getService(Ci.nsIRilMobileMessageDatabaseService);

let gUuidGenerator =
  Cc["@mozilla.org/uuid-generator;1"]
    .getService(Ci.nsIUUIDGenerator);

let gMmsService = Cc["@mozilla.org/mms/rilmmsservice;1"]
                       .getService(Ci.nsIMmsService);

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
        deferred.resolve(aDomMessage);
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

function testRetrieve(aCause, aInit, aCleanup) {
  log("testRetrieve: aCause = " + aCause);
  return Promise.resolve()
    .then(() => { if (aInit) aInit(); })
    .then(saveMmsNotification)
    .then((message) => retrieveMmsWithFailure(message.id))
    .then((response) => verifyErrorCause(response, aCause))
    .then(() => { if (aCleanup) aCleanup(); });
}

let setRadioDisabled = function(aDisabled) {
    log("set ril.radio.disabled to " + aDisabled);
    Services.prefs.setBoolPref("ril.radio.disabled", aDisabled);
};

testRetrieve(Ci.nsIMobileMessageCallback.RADIO_DISABLED_ERROR,
             setRadioDisabled.bind(null, true),
             setRadioDisabled.bind(null, false))
  .then(finish);