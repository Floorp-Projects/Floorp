/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const SENDER = "5555552368"; // the remote number
const RECEIVER = "15555215554"; // the emulator's number

const SHORT_BODY = "Hello SMS world!";
const LONG_BODY = new Array(17).join(SHORT_BODY);
ok(LONG_BODY.length > 160, "LONG_BODY.length");

function checkMessage(aMessage, aBody) {
  ok(aMessage instanceof SmsMessage, "Message is instanceof SmsMessage");

  is(aMessage.type, "sms", "message.type");
  ok(aMessage.id, "message.id");
  ok(aMessage.threadId, "message.threadId");
  ok(aMessage.iccId, "message.iccId");
  is(aMessage.delivery, "received", "message.delivery");
  is(aMessage.deliveryStatus, "success", "message.deliveryStatus");
  is(aMessage.sender, SENDER, "message.sender");
  is(aMessage.receiver, RECEIVER, "message.receiver");
  is(aMessage.body, aBody, "message.body");
  is(aMessage.messageClass, "normal", "message.messageClass");
  ok(aMessage.timestamp, "message.timestamp");
  is(aMessage.deliveryTimestamp, 0, "message.deliveryTimestamp");
  ok(aMessage.sentTimestamp, "message.sentTimestamp");
  is(aMessage.read, false, "message.read");
};

function test(aBody) {
  return sendTextSmsToEmulatorAndWait(SENDER, aBody)
    .then((aMessage) => checkMessage(aMessage, aBody));
}

startTestBase(function testCaseMain() {
  return ensureMobileMessage()
    .then(() => test(SHORT_BODY))
    .then(() => test(LONG_BODY));
});
