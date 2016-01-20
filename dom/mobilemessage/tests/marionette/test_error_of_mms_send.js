/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testSendFailed(aCause, aServiceId) {
  log("testSendFailed, aCause: " + aCause + ", aServiceId: " + aServiceId);
  let sendParameters;

  if (aServiceId) {
    sendParameters = { serviceId: aServiceId };
  }

  let testSubject = "Test";
  let testReceivers = ["+0987654321"];

  let mmsParameters = { subject: testSubject,
                        receivers: testReceivers,
                        attachments: [] };

  return sendMmsWithFailure(mmsParameters, sendParameters)
    .then((result) => {
      is(result.error.name, aCause, "Checking failure cause.");

      let domMessage = result.error.data;
      is(domMessage.id, result.message.id, "Checking message id.");
      is(domMessage.subject, testSubject, "Checking subject.");
      is(domMessage.receivers.length, testReceivers.length, "Checking no. of receivers.");
      for (let i = 0; i < testReceivers.length; i++) {
        is(domMessage.receivers[i], testReceivers[i], "Checking receiver address.");
      }

      let deliveryInfo = domMessage.deliveryInfo;
      is(deliveryInfo.length, testReceivers.length, "Checking no. of deliveryInfo.");
      for (let i = 0; i < deliveryInfo.length; i++) {
        is(deliveryInfo[i].receiver, testReceivers[i], "Checking receiver address.");
        is(deliveryInfo[i].deliveryStatus, "error", "Checking deliveryStatus.");
      }
    });
}

startTestCommon(function testCaseMain() {
  return Promise.resolve()
    .then(() => setAllRadioEnabled(false))
    .then(() => testSendFailed("RadioDisabledError"), 0)
    .then(() => setAllRadioEnabled(true))
    .then(() => runIfMultiSIM(
                  () => testSendFailed("NonActiveSimCardError", 1)));
});
