/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.setBoolPref("dom.sms.strict7BitEncoding", false);
SpecialPowers.setBoolPref("dom.sms.requestStatusReport", true);
SpecialPowers.addPermission("sms", true, document);

const SENDER = "15555215554"; // the emulator's number

let manager = window.navigator.mozMobileMessage;
ok(manager instanceof MozMobileMessageManager,
   "manager is instance of " + manager.constructor);

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
  ok(message instanceof MozSmsMessage,
     "message is instanceof " + message.constructor);

  ok(message.id, "message.id");
  ok(message.threadId, "message.threadId");
  is(message.delivery, delivery, "message.delivery");
  is(message.deliveryStatus, "pending", "message.deliveryStatus");
  is(message.sender, SENDER, "message.sender");
  ok(message.receiver, "message.receiver");
  is(message.body, body, "message.body");
  is(message.messageClass, "normal", "message.messageClass");
  ok(message.timestamp instanceof Date,
     "message.timestamp is instanceof " + message.timestamp.constructor);
  is(message.read, true, "message.read");
}

function doSendMessageAndCheckSuccess(receivers, body, callback) {
  let options = {};
  let now = Date.now();

  function done() {
    let rs = Array.isArray(receivers) ? receivers : [receivers];
    // Make sure we've send a message to each distinct receiver.
    for (let i = 0; i < rs.length; i++) {
      let opt = options[rs[i]];
      if (!(opt && opt.onSentCalled && opt.onRequestSuccessCalled)) {
        return;
      }
    }

    manager.removeEventListener("sending", onSmsSending);
    manager.removeEventListener("sent", onSmsSent);

    log("Done!");
    window.setTimeout(callback, 0);
  }

  function checkSentMessage(message, mark) {
    checkMessage(message, "sent", body);

    let receiver = message && message.receiver;
    if (!receiver) {
      ok(false, "message.receiver should be valid.");
      return;
    }

    let opt = options[receiver];
    if (!opt) {
      ok(false, "onsent should be called after onsending.");
      return;
    }

    let saved = opt.saved;
    is(message.id, saved.id, "message.id");
    is(message.receiver, saved.receiver, "message.receiver");
    is(message.body, saved.body, "message.body");
    is(message.timestamp.getTime(), saved.timestamp.getTime(),
       "the messages got from onsent event and request result must be the same");

    opt[mark] = true;

    done();
  }

  function onRequestSuccess(event) {
    log("request.onsuccess event received.");

    ok(event.target instanceof DOMRequest,
       "event.target is instanceof " + event.target.constructor);
    event.target.removeEventListener("success", onRequestSuccess);

    checkSentMessage(event.target.result, "onRequestSuccessCalled");
  }

  function onSmsSending(event) {
    log("onsending event received.");

    // Bug 838542: following check throws an exception and fails this case.
    // ok(event instanceof MozSmsEvent,
    //    "event is instanceof " + event.constructor)
    ok(event, "event is valid");

    let message = event.message;
    checkMessage(message, "sending", body);
    // SMSC timestamp is in seconds.
    ok(Math.floor(message.timestamp.getTime() / 1000) >= Math.floor(now / 1000),
       "sent timestamp is valid");

    let receiver = message.receiver;
    if (!receiver) {
      return;
    }

    if (options[receiver]) {
      ok(false, "duplicated onsending events found!");
      return;
    }

    options[receiver] = {
      saved: message,
      onSentCalled: false,
      onRequestSuccessCalled: false
    };
  }

  function onSmsSent(event) {
    log("onsent event received.");

    // Bug 838542: following check throws an exception and fails this case.
    // ok(event instanceof MozSmsEvent,
    //    "event is instanceof " + event.constructor)
    ok(event, "event is valid");

    checkSentMessage(event.message, "onSentCalled");
  }

  manager.addEventListener("sending", onSmsSending);
  manager.addEventListener("sent", onSmsSent);

  let result = manager.send(receivers, body);
  is(Array.isArray(result), Array.isArray(receivers),
     "send() returns an array of requests if receivers is an array");
  if (Array.isArray(receivers)) {
    is(result.length, receivers.length, "returned array length");
  } else {
    result = [result];
  }

  for (let i = 0; i < result.length; i++) {
    let request = result[i];
    ok(request instanceof DOMRequest,
       "request is instanceof " + request.constructor);
    request.addEventListener("success", onRequestSuccess);
  }
}

function testSendMessage() {
  log("Testing sending message to one receiver:");
  doSendMessageAndCheckSuccess("1", SHORT_BODY, testSendMultipartMessage);
}

function testSendMultipartMessage() {
  log("Testing sending message to one receiver:");
  doSendMessageAndCheckSuccess("1", LONG_BODY,
                               testSendMessageToMultipleRecipients);
}

function testSendMessageToMultipleRecipients() {
  log("Testing sending message to multiple receivers:");
  // TODO: bug 788928 - add test cases for ondelivered event.
  doSendMessageAndCheckSuccess(["1", "2"], SHORT_BODY, cleanUp);
}

function cleanUp() {
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  SpecialPowers.clearUserPref("dom.sms.strict7BitEncoding");
  SpecialPowers.clearUserPref("dom.sms.requestStatusReport");
  finish();
}

testSendMessage();
