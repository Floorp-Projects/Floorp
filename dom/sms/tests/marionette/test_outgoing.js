/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 20000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.setBoolPref("dom.sms.strict7BitEncoding", false);
SpecialPowers.addPermission("sms", true, document);
SpecialPowers.addPermission("mobileconnection", true, document);

let sms = window.navigator.mozSms;
let receiver = "5555552368";
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

function checkSentMessage(message, body, sentDate) {
  ok(message, "message is valid");
  ok(message instanceof MozSmsMessage,
     "message is instanceof " + message.constructor);

  ok(message.id, "message.id is valid");
  is(message.delivery, "sent", "message.delivery");
  is(message.deliveryStatus, "pending", "message.deliveryStatus");
  is(message.sender, null, "message.sender");
  is(message.receiver, receiver, "message.receiver");
  is(message.body, body, "message.body");
  ok(message.timestamp instanceof Date,
     "message.timestamp is instanceof " + message.timestamp.constructor);
  // SMSC timestamp is in seconds.
  ok(Math.floor(message.timestamp.getTime() / 1000) >= Math.floor(sentDate / 1000),
     "sent timestamp is valid");
  is(message.read, true, "message.read");
}

let sentMessages = null;
function checkSameSentMessage(message, body, now) {
  checkSentMessage(message, body, now);

  let sentMessage = sentMessages[message.id];
  if (!sentMessage) {
    sentMessages[message.id] = message;
    return;
  }

  // The two message instance should be exactly the same, but since we had
  // already checked most attributes of them, we only compare their
  // timestamps here.
  ok(sentMessage.timestamp.getTime() == message.timestamp.getTime(),
     "the messages got from onsent event and request result must be the same");
}

function doSendMessageAndCheckSuccess(receivers, body, callback) {
  sentMessages = [];

  let now = Date.now();
  let count;

  function done() {
    if (--count > 0) {
      return;
    }

    sms.removeEventListener("sent", onSmsSent);
    setTimeout(callback, 0);
  }

  function onSmsSent(event) {
    log("SmsManager.onsent event received.");

    ok(event instanceof MozSmsEvent,
       "event is instanceof " + event.constructor);
    // Event listener is removed in done().

    checkSameSentMessage(event.message, body, now);

    done();
  }

  function onRequestSuccess(event) {
    log("SmsRequest.onsuccess event received.");

    ok(event.target instanceof MozSmsRequest,
       "event.target is instanceof " + event.target.constructor);
    event.target.removeEventListener("success", onRequestSuccess);

    checkSameSentMessage(event.target.result, body, now);

    done();
  }

  sms.addEventListener("sent", onSmsSent);

  let result = sms.send(receivers, body);
  if (Array.isArray(receivers)) {
    ok(Array.isArray(result),
      "send() returns an array of requests if receivers is an array");
    is(result.length, receivers.length, "returned array length");

    // `receivers` is an array, so we have N request.onsuccess and N sms.onsent.
    count = result.length * 2;

    for (let i = 0; i < result.length; i++) {
      let request = result[i];
      ok(request instanceof MozSmsRequest,
         "request is instanceof " + request.constructor);
      request.addEventListener("success", onRequestSuccess);
    }

    return;
  }

  // `receivers` is not an array, so we have one request.onsuccess and one
  // sms.onsent.
  count = 2;

  let request = result;
  ok(request instanceof MozSmsRequest,
     "request is instanceof " + request.constructor);
  request.addEventListener("success", onRequestSuccess);
}

function testSendMessage() {
  log("Testing sending message to one receiver:");
  doSendMessageAndCheckSuccess(receiver, SHORT_BODY, testSendMultipartMessage);
}

function testSendMultipartMessage() {
  log("Testing sending message to one receiver:");
  doSendMessageAndCheckSuccess(receiver, LONG_BODY,
                               testSendMessageToMultipleRecipients);
}

function testSendMessageToMultipleRecipients() {
  log("Testing sending message to multiple receivers:");
  // TODO: bug 788928 - add test cases for nsIDOMSmsManager.ondelivered event
  doSendMessageAndCheckSuccess([receiver, receiver], SHORT_BODY, cleanUp);
}

function cleanUp() {
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.removePermission("mobileconnection", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  SpecialPowers.clearUserPref("dom.sms.strict7BitEncoding");
  finish();
}

waitFor(testSendMessage, function () {
  let connected = navigator.mozMobileConnection.voice.connected;
  if (!connected) {
    log("MobileConnection is not ready yet.");
  }
  return connected;
});

