/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const TEXT = "Incoming SMS message. Mozilla Firefox OS!";
const REMOTE = "5559997777";

function getNonExistentMsg(aId) {
  log("Attempting to get non-existent message (id: " + aId + ").");

  return getMessage(aId)
    .then(function onresolve() {
      ok(false, "request succeeded when tried to get non-existent sms");
    }, function onreject(aError) {
      ok(aError, "DOMError");
      is(aError.name, "NotFoundError", "error.name");
    });
}

function getMsgInvalidId(aId) {
  log("Attempting to get sms with invalid id (id: " + aId + ").");

  return getMessage(aId)
    .then(function onresolve() {
      ok(false, "request succeeded when tried to get message with " +
                "invalid id (id: " + aId + ").");
    }, function onreject(aError) {
      ok(aError, "DOMError");
      is(aError.name, "NotFoundError", "error.name");
    });
}

startTestBase(function testCaseMain() {
  let lastMessageId;

  return ensureMobileMessage()

    .then(() => sendTextSmsToEmulatorAndWait(REMOTE, TEXT))
    .then((aMessage) => { lastMessageId = aMessage.id; })

    .then(() => getNonExistentMsg(lastMessageId + 1))
    .then(() => getMsgInvalidId(-1))
    .then(() => getMsgInvalidId(0))

    .then(deleteAllMessages);
});
