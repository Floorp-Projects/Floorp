/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

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
    .then(() => setRadioEnabled(mobileConnection, false))
    .then(() => testSendFailed("RadioDisabledError"))
    .then(() => setRadioEnabled(mobileConnection, true));
});
