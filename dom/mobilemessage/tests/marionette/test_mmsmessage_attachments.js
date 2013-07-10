/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const MMS_MAX_LENGTH_SUBJECT = 40;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

let tasks = {
  // List of test fuctions. Each of them should call |tasks.next()| when
  // completed or |tasks.finish()| to jump to the last one.
  _tasks: [],
  _nextTaskIndex: 0,

  push: function push(func) {
    this._tasks.push(func);
  },

  next: function next() {
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

  finish: function finish() {
    this._tasks[this._tasks.length - 1]();
  },

  run: function run() {
    this.next();
  }
};

let mozMobileMessage;

function getAllMessages(callback, filter, reverse) {
  if (!filter) {
    filter = new MozSmsFilter;
  }
  let messages = [];
  let request = mozMobileMessage.getMessages(filter, reverse || false);
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

    let request = mozMobileMessage.delete(message.id);
    request.onsuccess = deleteAll.bind(null, messages);
    request.onerror = function (event) {
      ok(false, "failed to delete all messages");
      tasks.finish();
    }
  });
}

tasks.push(function () {
  log("Verifying initial state.");

  mozMobileMessage = window.navigator.mozMobileMessage;
  ok(mozMobileMessage instanceof MozMobileMessageManager);

  tasks.next();
});

tasks.push(function () {
  log("MmsMessage.attachments should be an empty array.");

  mozMobileMessage.onfailed = function (event) {
    mozMobileMessage.onfailed = null;

    let message = event.message;
    ok(Array.isArray(message.attachments) && message.attachments.length === 0,
       "message.attachments should be an empty array.");

    tasks.next();
  };

  // Have a long long subject causes the send fails, so we don't need
  // networking here.
  mozMobileMessage.sendMMS({
    subject: new Array(MMS_MAX_LENGTH_SUBJECT + 2).join("a"),
    receivers: ["1", "2"],
    attachments: [],
  });
});

tasks.push(deleteAllMessages);

// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
});

tasks.run();
