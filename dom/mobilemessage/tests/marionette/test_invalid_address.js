/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const MMS_MAX_LENGTH_SUBJECT = 40;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

var tasks = {
  // List of test fuctions. Each of them should call |tasks.next()| when
  // completed or |tasks.finish()| to jump to the last one.
  _tasks: [],
  _nextTaskIndex: 0,

  push: function(func) {
    this._tasks.push(func);
  },

  next: function() {
    let index = this._nextTaskIndex++;
    let task = this._tasks[index];
    try {
      task();
    } catch (ex) {
      ok(false, "test task[" + index + "] throws: " + ex);
      // Run last task as clean up if possible.
      if (index != this._tasks.length - 1) {
        this.finish();
      }
    }
  },

  finish: function() {
    this._tasks[this._tasks.length - 1]();
  },

  run: function() {
    this.next();
  }
};

var manager;

function getAllMessages(callback, filter, reverse) {
  let messages = [];
  let request = manager.getMessages(filter, reverse || false);
  request.onsuccess = function(event) {
    if (request.result) {
      messages.push(request.result);
      request.continue();
      return;
    }

    window.setTimeout(callback.bind(null, messages), 0);
  }
}

function deleteAllMessages() {
  getAllMessages(function deleteAll(messages) {
    let message = messages.shift();
    if (!message) {
      ok(true, "all messages deleted");
      tasks.next();
      return;
    }

    let request = manager.delete(message.id);
    request.onsuccess = deleteAll.bind(null, messages);
    request.onerror = function(event) {
      ok(false, "failed to delete all messages");
      tasks.finish();
    }
  });
}

function testInvalidAddressForSMS(aInvalidAddr)  {
  log("manager.send(...) should get 'InvalidAddressError' error " +
      "when attempting to send SMS to: " + aInvalidAddr);

  let request = manager.send(aInvalidAddr, "Test");

  request.onerror = function(event) {
    log("Received 'onerror' DOMRequest event.");
    let error = event.target.error;
    ok(error instanceof DOMError, "should be a valid DOMError object");
    ok(error.name === "InvalidAddressError", "should be 'InvalidAddressError'");
    tasks.next();
  };
}

function testInvalidAddressForMMS(aInvalidAddrs)  {
  log("manager.sendMMS(...) should get 'InvalidAddressError' error " +
      "when attempting to send MMS to: " + aInvalidAddrs);

  let request = manager.sendMMS({
    subject: "Test",
    receivers: aInvalidAddrs,
    attachments: [],
  });

  request.onerror = function(event) {
    log("Received 'onerror' DOMRequest event.");
    let error = event.target.error;
    ok(error instanceof DOMError, "should be a valid DOMError object");
    ok(error.name === "InvalidAddressError", "should be 'InvalidAddressError'");
    tasks.next();
  };
}

tasks.push(function() {
  log("Verifying initial state.");

  manager = window.navigator.mozMobileMessage;
  ok(manager instanceof MozMobileMessageManager,
     "manager is instance of " + manager.constructor);

  tasks.next();
});

// Test sending SMS to invalid addresses.
tasks.push(testInvalidAddressForSMS.bind(this, "&%&"));
tasks.push(testInvalidAddressForSMS.bind(this, ""));

// Test sending MMS to invalid addresses.
tasks.push(testInvalidAddressForMMS.bind(this, ["&%&"]));
tasks.push(testInvalidAddressForMMS.bind(this, [""]));
tasks.push(testInvalidAddressForMMS.bind(this, ["123", "&%&", "456"]));

tasks.push(deleteAllMessages);

// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
});

tasks.run();
