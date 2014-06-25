/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const REMOTE = "5559997777"; // the remote number
const EMULATOR = "15555215554"; // the emulator's number

const IN_TEXT = "Incoming SMS message. Mozilla Firefox OS!";
const OUT_TEXT = "Outgoing SMS message. Mozilla Firefox OS!";

startTestBase(function testCaseMain() {
  let incomingSms, outgoingSms;

  return ensureMobileMessage()

    .then(() => sendTextSmsToEmulatorAndWait(REMOTE, IN_TEXT))
    .then((aMessage) => { incomingSms = aMessage; })

    .then(() => sendSmsWithSuccess(REMOTE, OUT_TEXT))
    .then((aMessage) => { outgoingSms = aMessage; })

    .then(() => getMessage(incomingSms.id))
    .then((aMessage) => compareSmsMessage(aMessage, incomingSms))

    .then(() => getMessage(outgoingSms.id))
    .then((aMessage) => compareSmsMessage(aMessage, outgoingSms))

    .then(deleteAllMessages);
});
