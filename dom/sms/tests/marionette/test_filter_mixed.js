/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 40000;

const SELF = 5554;
const NUM_THREADS = 10;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

let sms = window.navigator.mozSms;
ok(sms instanceof MozSmsManager);

let pendingEmulatorCmdCount = 0;
function sendSmsToEmulator(from, text) {
  ++pendingEmulatorCmdCount;

  let cmd = "sms send " + from + " " + text;
  runEmulatorCmd(cmd, function (result) {
    --pendingEmulatorCmdCount;

    is(result[0], "OK", "Emulator response");
  });
}

function getAllMessages(callback, filter, reverse) {
  if (!filter) {
    filter = new MozSmsFilter;
  }
  let messages = [];
  let request = sms.getMessages(filter, reverse || false);
  request.onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor.message) {
      messages.push(cursor.message);
      cursor.continue();
      return;
    }

    window.setTimeout(callback.bind(null, messages), 0);
  }
}

function deleteAllMessages(next) {
  getAllMessages(function deleteAll(messages) {
    let message = messages.shift();
    if (!message) {
      ok(true, "all messages deleted");
      window.setTimeout(next, 0);
      return;
    }

    let request = sms.delete(message.id);
    request.onsuccess = deleteAll.bind(null, messages);
    request.onerror = function (event) {
      ok(false, "failed to delete all messages");
      window.setTimeout(cleanUp, 0);
    }
  });
}

/**
 * Populate SmsDatabase with messages to being tests. We'll have NUM_THREADS
 * sent and received messages, and NUM_THREADS/2 unread received messages.
 *
 *   send    to   "0"
 *   receive from "0", count = 1
 *   mark         received as read
 *
 *   send    to   "1"
 *   receive from "1", count = 2
 *
 *   send    to   "2"
 *   receive from "2", count = 3
 *   mark         received as read
 *   ...
 *   send    to   "9"
 *   receive from "9", count = 10
 */
function populateMessages() {
  let count = 0;

  function sendMessage(iter) {
    let request = sms.send("" + iter, "Nice to meet you");
    request.onsuccess = function onRequestSuccess(event) {
      sms.addEventListener("received", onReceived);
      sendSmsToEmulator("" + iter, "Nice to meet you, too");
    }
    request.onerror = function onRequestError(event) {
      window.setTimeout(deleteAllMessages.bind(null, cleanUp), 0);
    }
  }

  function onReceived(event) {
    sms.removeEventListener("received", onReceived);

    ++count;
    if (count % 2) {
      let request = sms.markMessageRead(event.message.id, true);
      request.onsuccess = function onRequestSuccess(event) {
        if (count < NUM_THREADS) {
          sendMessage(count);
        } else {
          window.setTimeout(testDeliveryAndNumber, 0);
        }
      }
      request.onerror = function onRequestError(event) {
        window.setTimeout(deleteAllMessages.bind(null, cleanUp), 0);
      }
    } else if (count < NUM_THREADS) {
      sendMessage(count);
    } else {
      window.setTimeout(testDeliveryAndNumber, 0);
    }
  }

  sendMessage(count);
}

function testDeliveryAndNumber() {
  log("Checking delivery == sent && number == 0");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.numbers = ["0"];
  getAllMessages(function (messages) {
    // Only { delivery: "sent", receiver: "0", read: true }
    is(messages.length, 1, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.delivery, filter.delivery, "message delivery");
      if (!((message.sender == filter.numbers[0])
            || (message.receiver == filter.numbers[0]))) {
        ok(false, "message sendor or receiver number");
      }
    }

    getAllMessages(function (messages_r) {
      is(messages.length, messages_r.length, "message count");
      for (let i = 0; i < messages_r.length; i++) {
        is(messages_r[i].id, messages[messages.length - 1 - i].id, "message id");
      }

      window.setTimeout(testDeliveryAndNumberNotFound, 0);
    }, filter, true);
  }, filter);
}

function testDeliveryAndNumberNotFound() {
  log("Checking delivery == sent && number == 12345");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.numbers = ["12345"];
  getAllMessages(function (messages) {
    is(messages.length, 0, "message count");

    window.setTimeout(testDeliveryAndRead, 0);
  }, filter);
}

function testDeliveryAndRead() {
  log("Checking delivery == received && read == true");
  let filter = new MozSmsFilter();
  filter.delivery = "received";
  filter.read = true;
  getAllMessages(function (messages) {
    // { delivery: "received", sender: "0", read: true },
    // { delivery: "received", sender: "2", read: true },
    // { delivery: "received", sender: "4", read: true },
    // { delivery: "received", sender: "6", read: true }, and
    // { delivery: "received", sender: "8", read: true },
    is(messages.length, NUM_THREADS / 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.delivery, filter.delivery, "message delivery");
      is(message.read, filter.read, "message read");
    }

    getAllMessages(function (messages_r) {
      is(messages.length, messages_r.length, "message count");
      for (let i = 0; i < messages_r.length; i++) {
        is(messages_r[i].id, messages[messages.length - 1 - i].id, "message id");
      }

      window.setTimeout(testDeliveryAndReadNotFound, 0);
    }, filter, true);
  }, filter);
}

function testDeliveryAndReadNotFound() {
  log("Checking delivery == sent && read == false");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.read = false;
  getAllMessages(function (messages) {
    is(messages.length, 0, "message count");

    window.setTimeout(testNumberAndRead, 0);
  }, filter);
}

function testNumberAndRead() {
  log("Checking number == 0 && read == true");
  let filter = new MozSmsFilter();
  filter.numbers = ["0"];
  filter.read = true;
  getAllMessages(function (messages) {
    // { delivery: "sent", receiver: "0", read: true }, and
    // { delivery: "received", sender: "0", read: true }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      if (!((message.sender == filter.numbers[0])
            || (message.receiver == filter.numbers[0]))) {
        ok(false, "message sendor or receiver number");
      }
      is(message.read, filter.read, "message read");
    }

    getAllMessages(function (messages_r) {
      is(messages.length, messages_r.length, "message count");
      for (let i = 0; i < messages_r.length; i++) {
        is(messages_r[i].id, messages[messages.length - 1 - i].id, "message id");
      }

      window.setTimeout(testNumberAndReadNotFound, 0);
    }, filter, true);
  }, filter);
}

function testNumberAndReadNotFound() {
  log("Checking number == 12345 && read == true");
  let filter = new MozSmsFilter();
  filter.numbers = ["12345"];
  filter.read = true;
  getAllMessages(function (messages) {
    is(messages.length, 0, "message count");

    window.setTimeout(testMultipleNumbers, 0);
  }, filter);
}

function testMultipleNumbers() {
  log("Checking number == 0 || number == 1");
  let filter = new MozSmsFilter();
  filter.numbers = ["0", "1"];
  getAllMessages(function (messages) {
    // { delivery: "sent", receiver: "0", read: true }
    // { delivery: "received", sender: "0", read: true }
    // { delivery: "sent", receiver: "1", read: true }
    // { delivery: "received", sender: "1", read: false }
    is(messages.length, 4, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      if (!((message.sender == filter.numbers[0])
            || (message.receiver == filter.numbers[0])
            || (message.sender == filter.numbers[1])
            || (message.receiver == filter.numbers[1]))) {
        ok(false, "message sendor or receiver number");
      }
    }

    window.setTimeout(testMultipleNumbersNotFound, 0);
  }, filter);
}

function testMultipleNumbersNotFound() {
  log("Checking number == 12345 || number == 6789");
  let filter = new MozSmsFilter();
  filter.numbers = ["12345", "6789"];
  getAllMessages(function (messages) {
    is(messages.length, 0, "message count");

    window.setTimeout(testDeliveryAndMultipleNumbers, 0);
  }, filter);
}

function testDeliveryAndMultipleNumbers() {
  log("Checking delivery == sent && (number == 0 || number == 1)");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.numbers = ["0", "1"];
  getAllMessages(function (messages) {
    // { delivery: "sent", receiver: "0", read: true }
    // { delivery: "sent", receiver: "1", read: true }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.delivery, filter.delivery, "message delivery");
      if (!((message.sender == filter.numbers[0])
            || (message.receiver == filter.numbers[0])
            || (message.sender == filter.numbers[1])
            || (message.receiver == filter.numbers[1]))) {
        ok(false, "message sendor or receiver number");
      }
    }

    window.setTimeout(testMultipleNumbersAndRead, 0);
  }, filter);
}

function testMultipleNumbersAndRead() {
  log("Checking (number == 0 || number == 1) && read == true");
  let filter = new MozSmsFilter();
  filter.numbers = ["0", "1"];
  filter.read = true;
  getAllMessages(function (messages) {
    // { delivery: "sent", receiver: "0", read: true }
    // { delivery: "received", sender: "0", read: true }
    // { delivery: "sent", receiver: "1", read: true }
    is(messages.length, 3, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.read, filter.read, "message read");
      if (!((message.sender == filter.numbers[0])
            || (message.receiver == filter.numbers[0])
            || (message.sender == filter.numbers[1])
            || (message.receiver == filter.numbers[1]))) {
        ok(false, "message sendor or receiver number");
      }
    }

    window.setTimeout(deleteAllMessages.bind(null, cleanUp), 0);
  }, filter);
}

function cleanUp() {
  if (pendingEmulatorCmdCount) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
}

deleteAllMessages(populateMessages);
