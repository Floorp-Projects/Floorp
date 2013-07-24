/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

// Copied from dom/system/gonk/ril_consts.js.
const PDU_MAX_USER_DATA_7BIT = 160;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

let manager = window.navigator.mozMobileMessage;
ok(manager instanceof MozMobileMessageManager, "mozMobileMessage");

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

function addTest(text, segments, charsPerSegment, charsAvailableInLastSegment) {
  tasks.push(function () {
    log("Testing '" + text + "' ...");
    let info = manager.getSegmentInfoForText(text);
    is(info.segments, segments, "info.segments");
    is(info.charsPerSegment, charsPerSegment, "info.charsPerSegment");
    is(info.charsAvailableInLastSegment, charsAvailableInLastSegment,
       "info.charsAvailableInLastSegment");

    tasks.next();
  });
}

function addTestThrows(text) {
  tasks.push(function () {
    log("Testing '" + text + "' ...");
    try {
      let info = manager.getSegmentInfoForText(text);

      ok(false, "Not thrown");
      tasks.finish();
    } catch (e) {
      tasks.next();
    }
  });
}

addTestThrows(null);

// Testing "undefined".
addTest(undefined, 1, PDU_MAX_USER_DATA_7BIT,
        PDU_MAX_USER_DATA_7BIT - "undefined".length);

// Testing numeric values.
addTest(0,   1, PDU_MAX_USER_DATA_7BIT, PDU_MAX_USER_DATA_7BIT - "0".length);
addTest(1.0, 1, PDU_MAX_USER_DATA_7BIT, PDU_MAX_USER_DATA_7BIT - "1".length);

// Testing empty object.  The empty object extends to "[object Object]" and both
// '[' and ']' are in default single shift table, so each of them takes two
// septets.
addTest({}, 1, PDU_MAX_USER_DATA_7BIT,
        PDU_MAX_USER_DATA_7BIT - (("" + {}).length + 2));

// Testing Date object.
let date = new Date();
addTest(date, 1, PDU_MAX_USER_DATA_7BIT,
        PDU_MAX_USER_DATA_7BIT - ("" + date).length);

addTest("", 0, PDU_MAX_USER_DATA_7BIT,
        PDU_MAX_USER_DATA_7BIT - "".length);

// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
});

tasks.run();
