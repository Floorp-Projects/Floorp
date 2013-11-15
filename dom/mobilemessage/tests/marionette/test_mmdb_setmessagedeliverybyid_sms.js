/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const RIL_MOBILEMESSAGEDATABASESERVICE_CONTRACTID =
  "@mozilla.org/mobilemessage/rilmobilemessagedatabaseservice;1";

const RECEIVER = "+1234567890";
const TEXT = "The quick brown fox jumps over the lazy dog.";

const DELIVERY_SENDING = "sending";
const DELIVERY_SENT = "sent";
const DELIVERY_RECEIVED = "received";
const DELIVERY_NOT_DOWNLOADED = "not-downloaded";
const DELIVERY_ERROR = "error";

const DELIVERY_STATUS_NOT_APPLICABLE = "not-applicable";
const DELIVERY_STATUS_SUCCESS = "success";
const DELIVERY_STATUS_PENDING = "pending";
const DELIVERY_STATUS_ERROR = "error";

let dbService;

/**
 * @param aMessageId
 * @param aParams
 *        An array of four elements [<delivery>, <deliveryStatus>,
 *        <expected delivery>, <expected deliveryStatus>].
 */
function setMessageDeliveryByMessageId(aMessageId, aParams) {
  if (!dbService) {
    dbService = Cc[RIL_MOBILEMESSAGEDATABASESERVICE_CONTRACTID]
                .getService(Ci.nsIRilMobileMessageDatabaseService);
    if (!dbService) {
      log("  Failed to get database service.");
      return Promise.reject();
    }
  }

  let deferred = Promise.defer();

  log("  Set to " + aParams[0] + ":" + aParams[1]);
  dbService.setMessageDeliveryByMessageId(aMessageId, null, aParams[0],
                                          aParams[1], null,
                                          function(aRv, aDomMessage) {
    if (aRv !== Cr.NS_OK) {
      deferred.reject(aRv);
      return;
    }

    is(aDomMessage.delivery, aParams[2], "message.delivery");
    is(aDomMessage.deliveryStatus, aParams[3], "message.deliveryStatus");

    deferred.resolve([aRv, aDomMessage]);
  });

  return deferred.promise;
}

function test(aTitle, aParamArray) {
  log(aTitle);

  return sendSmsWithSuccess(RECEIVER, TEXT)
    .then(function(aDomMessage) {
      let id = aDomMessage.id;

      let promise = Promise.resolve();
      while (aParamArray.length) {
        let params = aParamArray.shift();
        promise =
          promise.then(setMessageDeliveryByMessageId.bind(null, id, params));
      }

      return promise;
    });
}

startTestCommon(function testCaseMain() {
  return Promise.resolve()
    .then(test.bind(null, "Simulate send failed without delivery report requisition", [
      [DELIVERY_SENDING, DELIVERY_STATUS_NOT_APPLICABLE,
       DELIVERY_SENDING, DELIVERY_STATUS_NOT_APPLICABLE],
      [DELIVERY_ERROR, DELIVERY_STATUS_ERROR,
       DELIVERY_ERROR, DELIVERY_STATUS_ERROR],
    ]))
    .then(test.bind(null, "Simulate send failed with delivery report requisition", [
      [DELIVERY_SENDING, DELIVERY_STATUS_PENDING,
       DELIVERY_SENDING, DELIVERY_STATUS_PENDING],
      [DELIVERY_ERROR, DELIVERY_STATUS_ERROR,
       DELIVERY_ERROR, DELIVERY_STATUS_ERROR],
    ]))
    .then(test.bind(null, "Simulate sent without delivery report requisition", [
      [DELIVERY_SENDING, DELIVERY_STATUS_NOT_APPLICABLE,
       DELIVERY_SENDING, DELIVERY_STATUS_NOT_APPLICABLE],
      [DELIVERY_SENT, null,
       DELIVERY_SENT, DELIVERY_STATUS_NOT_APPLICABLE],
    ]))
    .then(test.bind(null, "Simulate sent with delivery report success", [
      [DELIVERY_SENDING, DELIVERY_STATUS_PENDING,
       DELIVERY_SENDING, DELIVERY_STATUS_PENDING],
      [DELIVERY_SENT, null,
       DELIVERY_SENT, DELIVERY_STATUS_PENDING],
      [null, DELIVERY_STATUS_SUCCESS,
       DELIVERY_SENT, DELIVERY_STATUS_SUCCESS],
    ]))
    .then(test.bind(null, "Simulate sent with delivery report error", [
      [DELIVERY_SENDING, DELIVERY_STATUS_PENDING,
       DELIVERY_SENDING, DELIVERY_STATUS_PENDING],
      [DELIVERY_SENT, null,
       DELIVERY_SENT, DELIVERY_STATUS_PENDING],
      [null, DELIVERY_ERROR,
       DELIVERY_SENT, DELIVERY_ERROR],
    ]));
});
