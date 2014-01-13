/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("sms", true, document);

// Expected SMSC addresses of emulator
const SMSC = "\"+123456789\",145";

let manager = window.navigator.mozMobileMessage;

let tasks = {
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

tasks.push(function init() {
  log("Initialize test object.");
  ok(manager instanceof MozMobileMessageManager,
     "manager is instance of " + manager.constructor);
  tasks.next();
});

tasks.push(function readSmscAddress() {
  log("read SMSC address");

  let req = manager.getSmscAddress();
  ok(req, "DOMRequest object for getting smsc address");

  req.onsuccess = function(e) {
    is(e.target.result, SMSC, "SMSC address");
    tasks.next();
  };

  req.onerror = function(error) {
    ok(false, "readSmscAddress(): Received 'onerror'");
    tasks.finish();
  };
});

// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  manager.onreceived = null;
  SpecialPowers.removePermission("sms", document);
  finish();
});

// Start the test
tasks.run();
