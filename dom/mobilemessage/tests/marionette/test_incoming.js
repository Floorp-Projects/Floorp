/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

const SENDER = "5555552368"; // the remote number
const RECEIVER = "15555215554"; // the emulator's number

let manager = window.navigator.mozMobileMessage;
ok(manager instanceof MozMobileMessageManager,
   "manager is instance of " + manager.constructor);

let body = "Hello SMS world!";

let completed = false;
runEmulatorCmd("sms send " + SENDER + " " + body, function(result) {
  log("Sent fake SMS: " + result);
  is(result[0], "OK", "Emulator command result");
  completed = true;
});

manager.onreceived = function onreceived(event) {
  log("Received an SMS!");

  let message = event.message;
  ok(message instanceof MozSmsMessage, "Message is instanceof MozSmsMessage");

  ok(message.threadId, "thread id");
  is(message.delivery, "received", "Message delivery");
  is(message.deliveryStatus, "success", "Delivery status");
  is(message.sender, SENDER, "Message sender");
  is(message.receiver, RECEIVER, "Message receiver");
  is(message.body, body, "Message body");
  is(message.messageClass, "normal", "Message class");
  ok(message.timestamp instanceof Date, "Message timestamp is a date");
  ok(message.deliveryTimestamp === null, "Message deliveryTimestamp is null");

  cleanUp();
};

function cleanUp() {
  if (!completed) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("sms", document);
  finish();
}
