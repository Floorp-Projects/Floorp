/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'mmdb_head.js';

const DBNAME = "test_mmdb_setmessagedeliverybyid_sms:" + newUUID();

const RECEIVER = "+1234567890";
const TEXT = "The quick brown fox jumps over the lazy dog.";

const MESSAGE_STORE_NAME = "sms";

const DELIVERY_SENDING = "sending";
const DELIVERY_SENT = "sent";
const DELIVERY_RECEIVED = "received";
const DELIVERY_NOT_DOWNLOADED = "not-downloaded";
const DELIVERY_ERROR = "error";

const DELIVERY_STATUS_NOT_APPLICABLE = "not-applicable";
const DELIVERY_STATUS_SUCCESS = "success";
const DELIVERY_STATUS_PENDING = "pending";
const DELIVERY_STATUS_ERROR = "error";

function verify(aMmdb, aMessageId, aDelivery, aDeliveryStatus, aRv, aDomMessage) {
  log("  Verify(" + aMessageId + ", " + aDelivery + ", " + aDeliveryStatus + ")");

  ok(Components.isSuccessCode(aRv), "Components.isSuccessCode(" + aRv + ")");
  ok(aDomMessage, "DOM message validity");
  is(aDomMessage.delivery, aDelivery, "message.delivery");
  is(aDomMessage.deliveryStatus, aDeliveryStatus, "message.deliveryStatus");

  let deferred = Promise.defer();

  // Verify deliveryIndex, sentTimestamp and deliveryTimestamp
  aMmdb.newTxn("readonly", function(aError, aTransaction, aMessageStore) {
    let messageRecord;
    aTransaction.oncomplete = function() {
      ok(true, "transaction complete");

      is(messageRecord.deliveryIndex[0], aDelivery,
         "messageRecord.deliveryIndex[0]");
      is(messageRecord.deliveryIndex[1], messageRecord.timestamp,
         "messageRecord.deliveryIndex[1]");
      if (aDelivery == DELIVERY_SENT) {
        ok(messageRecord.sentTimestamp >= messageRecord.timestamp,
           "messageRecord.sentTimestamp");
      }
      if (aDeliveryStatus == DELIVERY_STATUS_SUCCESS) {
        ok(messageRecord.deliveryTimestamp >= messageRecord.timestamp,
           "messageRecord.deliveryTimestamp");
      }

      deferred.resolve(aMmdb);
    };

    aMessageStore.get(aMessageId).onsuccess = function(event) {
      messageRecord = event.target.result;
      ok(true, "Got messageRecord " + messageRecord.id);
    };
  }, [MESSAGE_STORE_NAME]);

  return deferred.promise;
}

function test(aTitle, aMmdb, aDeliveryStatusRequested, aParamArray) {
  log(aTitle);

  let message = {
    type: "sms",
    sender: null,
    timestamp: Date.now(),
    deliveryStatusRequested: aDeliveryStatusRequested,
    receiver: RECEIVER,
    iccId: null,
    body: TEXT,
  };
  return saveSendingMessage(aMmdb, message)
    .then(function(aValues) {
      let [resultCode, domMessage] = aValues;
      let promise =
        verify(aMmdb, domMessage.id, DELIVERY_SENDING,
               (aDeliveryStatusRequested ? DELIVERY_STATUS_PENDING
                                         : DELIVERY_STATUS_NOT_APPLICABLE),
               resultCode, domMessage);
      while (aParamArray.length) {
        let params = aParamArray.shift();
        let v = verify.bind(null, aMmdb, domMessage.id, params[2], params[3]);
        promise = promise
          .then(() => {
            log("  Set to " + params[0] + ":" + params[1]);
            return setMessageDeliveryByMessageId(aMmdb, domMessage.id, null,
                                                 params[0], params[1], null);
          })
          .then((aValues) => v(aValues[0], aValues[1]));
      }

      return promise;
    });
}

startTestBase(function testCaseMain() {
  return initMobileMessageDB(newMobileMessageDB(), DBNAME, 0)
    .then((aMmdb) => test("Simulate send failed without delivery report requisition",
                          aMmdb, false, [
      [DELIVERY_ERROR, DELIVERY_STATUS_ERROR,
       DELIVERY_ERROR, DELIVERY_STATUS_ERROR],
    ]))
    .then((aMmdb) => test("Simulate send failed with delivery report requisition",
                          aMmdb, true, [
      [DELIVERY_ERROR, DELIVERY_STATUS_ERROR,
       DELIVERY_ERROR, DELIVERY_STATUS_ERROR],
    ]))
    .then((aMmdb) => test("Simulate sent without delivery report requisition",
                          aMmdb, false, [
      [DELIVERY_SENT, null,
       DELIVERY_SENT, DELIVERY_STATUS_NOT_APPLICABLE],
    ]))
    .then((aMmdb) => test("Simulate sent with delivery report success",
                          aMmdb, true, [
      [DELIVERY_SENT, null,
       DELIVERY_SENT, DELIVERY_STATUS_PENDING],
      [null, DELIVERY_STATUS_SUCCESS,
       DELIVERY_SENT, DELIVERY_STATUS_SUCCESS],
    ]))
    .then((aMmdb) => test("Simulate sent with delivery report error",
                          aMmdb, true, [
      [DELIVERY_SENT, null,
       DELIVERY_SENT, DELIVERY_STATUS_PENDING],
      [null, DELIVERY_ERROR,
       DELIVERY_SENT, DELIVERY_ERROR],
    ]))
    .then(closeMobileMessageDB);
});
