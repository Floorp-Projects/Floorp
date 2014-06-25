/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const NUMBER_OF_MESSAGES = 10;
const REMOTE = "5552229797";

function simulateIncomingSms() {
  let promise = Promise.resolve();
  let messages = [];

  for (let i = 0; i< NUMBER_OF_MESSAGES; i++) {
    let text = "Incoming SMS number " + i;
    promise = promise.then(() => sendTextSmsToEmulatorAndWait(REMOTE, text))
      .then(function(aMessage) {
        messages.push(aMessage);
        return messages;
      });
  }

  return promise;
}

function verifyFoundMsgs(foundSmsList, smsList) {
  for (let x = 0; x < NUMBER_OF_MESSAGES; x++) {
    compareSmsMessage(foundSmsList[x], smsList[x]);
  }

  log("Content in all of the returned SMS messages is correct.");
}

startTestCommon(function testCaseMain() {
  let incomingMessages;

  return simulateIncomingSms()
    .then((aMessages) => { incomingMessages = aMessages; })

    .then(() => getMessages(null, false))
    .then((aFoundMessages) => verifyFoundMsgs(aFoundMessages, incomingMessages))

    .then(() => getMessages(null, true))
    .then((aFoundMessages) => verifyFoundMsgs(aFoundMessages,
                                              incomingMessages.slice().reverse()));
});
