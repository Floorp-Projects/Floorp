/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const SENDER = "15555215554"; // the emulator's number

const SHORT_BODY = "Hello SMS world!";
const LONG_BODY = "Let me not to the marriage of true minds\n"
                + "Admit impediments. Love is not love\n"
                + "Which alters when it alteration finds,\n"
                + "Or bends with the remover to remove:\n\n"
                + "O, no! it is an ever-fix`ed mark,\n"
                + "That looks on tempests and is never shaken;\n"
                + "It is the star to every wand'ring bark,\n"
                + "Whose worth's unknown, although his heighth be taken.\n\n"
                + "Love's not Time's fool, though rosy lips and cheeks\n"
                + "Within his bending sickle's compass come;\n"
                + "Love alters not with his brief hours and weeks,\n"
                + "But bears it out even to the edge of doom:\n\n"
                + "If this be error and upon me proved,\n"
                + "I never writ, nor no man ever loved. ";

function checkMessage(message, delivery, body) {
  ok(message, "message is valid");
  ok(message instanceof SmsMessage,
     "message is instanceof " + message.constructor);

  is(message.type, "sms", "message.type");
  ok(message.id, "message.id");
  ok(message.threadId, "message.threadId");
  ok(message.iccId, "message.iccId");
  is(message.delivery, delivery, "message.delivery");
  is(message.deliveryStatus, "pending", "message.deliveryStatus");
  is(message.sender, SENDER, "message.sender");
  ok(message.receiver, "message.receiver");
  is(message.body, body, "message.body");
  is(message.messageClass, "normal", "message.messageClass");
  is(message.read, true, "message.read");

  // TODO: bug 788928 - add test cases for deliverysuccess event.
  is(message.deliveryTimestamp, 0, "deliveryTimestamp is 0");

  // Test message.sentTimestamp.
  if (message.delivery == "sending") {
    ok(message.sentTimestamp == 0, "message.sentTimestamp should be 0");
  } else if (message.delivery == "sent") {
    ok(message.sentTimestamp != 0, "message.sentTimestamp shouldn't be 0");
  }
}

function isReceiverMatch(aReceiver, aEvent) {
  // Bug 838542: following check throws an exception and fails this case.
  // ok(event instanceof MozSmsEvent,
  //    "event is instanceof " + event.constructor)
  ok(aEvent, "sending event is valid");

  let message = aEvent.message;
  return message.receiver === aReceiver;
}

function doSingleRequest(aRequest, aReceiver, aBody, aNow) {
  let sendingGot = false, sentGot = false, successGot = false;
  let sendingMessage;
  let promises = [];

  promises.push(waitForManagerEvent("sending",
                                    isReceiverMatch.bind(null, aReceiver))
    .then(function(aEvent) {
      log("  onsending event for '" + aReceiver + "' received.");

      sendingMessage = aEvent.message;
      checkMessage(sendingMessage, "sending", aBody);
      // timestamp is in seconds.
      ok(Math.floor(sendingMessage.timestamp / 1000) >= Math.floor(aNow / 1000),
         "sent timestamp is valid");

      ok(!sendingGot, "sending event should not have been triggered");
      ok(!sentGot, "sent event should not have been triggered");
      ok(!successGot, "success event should not have been triggered");

      sendingGot = true;
    }));

  promises.push(waitForManagerEvent("sent",
                                    isReceiverMatch.bind(null, aReceiver))
    .then(function(aEvent) {
      log("  onsent event for '" + aReceiver + "' received.");

      let message = aEvent.message;
      checkMessage(message, "sent", aBody);
      // Should be mostly identical to sendingMessage.
      is(message.id, sendingMessage.id, "message.id");
      is(message.receiver, sendingMessage.receiver, "message.receiver");
      is(message.body, sendingMessage.body, "message.body");
      is(message.timestamp, sendingMessage.timestamp, "message.timestamp");

      ok(sendingGot, "sending event should have been triggered");
      ok(!sentGot, "sent event should not have been triggered");
      ok(successGot, "success event should have been triggered");

      sentGot = true;
    }));

  promises.push(aRequest.then(function(aResult) {
    log("  onsuccess event for '" + aReceiver + "' received.");

    checkMessage(aResult, "sent", aBody);
    // Should be mostly identical to sendingMessage.
    is(aResult.id, sendingMessage.id, "message.id");
    is(aResult.receiver, sendingMessage.receiver, "message.receiver");
    is(aResult.body, sendingMessage.body, "message.body");
    is(aResult.timestamp, sendingMessage.timestamp, "message.timestamp");

    ok(sendingGot, "sending event should have been triggered");
    ok(!sentGot, "sent event should not have been triggered");
    ok(!successGot, "success event should not have been triggered");

    successGot = true;
  }));

  return Promise.all(promises);
}

function doSendMessageAndCheckSuccess(receivers, body) {
  log("Testing sending message(s) to receiver(s): " + JSON.stringify(receivers));

  let now = Date.now();

  let result = manager.send(receivers, body);
  is(Array.isArray(result), Array.isArray(receivers),
     "send() returns an array of requests if receivers is an array");
  if (Array.isArray(receivers)) {
    is(result.length, receivers.length, "returned array length");

    return Promise.all(result.map(function(request, index) {
      return doSingleRequest(request, receivers[index], body, now);
    }));
  }

  return doSingleRequest(result, receivers, body, now);
}

startTestBase(function testCaseMain() {
  return ensureMobileMessage()
    .then(() => pushPrefEnv({ set: [['dom.sms.strict7BitEncoding', false],
                                    ['dom.sms.requestStatusReport', true]] }))
    .then(() => doSendMessageAndCheckSuccess("1", SHORT_BODY))
    .then(() => doSendMessageAndCheckSuccess("1", LONG_BODY))
    .then(() => doSendMessageAndCheckSuccess(["1", "2"], SHORT_BODY));
});
