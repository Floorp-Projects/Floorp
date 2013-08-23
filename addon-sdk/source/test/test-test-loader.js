/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const { LoaderWithHookedConsole } = require("sdk/test/loader");

exports["test LoaderWithHookedConsole"] = function (assert) {
  let count = 0;
  function onMessage(type, message) {
    switch (count++) {
      case 0:
        assert.equal(type, "log", "got log type");
        assert.equal(message, "1st", "got log msg");
        break;
      case 1:
        assert.equal(type, "error", "got error type");
        assert.equal(message, "2nd", "got error msg");
        break;
      case 2:
        assert.equal(type, "warn", "got warn type");
        assert.equal(message, "3rd", "got warn msg");
        break;
      case 3:
        assert.equal(type, "info", "got info type");
        assert.equal(message, "4th", "got info msg");
        break;
      case 4:
        assert.equal(type, "debug", "got debug type");
        assert.equal(message, "5th", "got debug msg");
        break;
      case 5:
        assert.equal(type, "exception", "got exception type");
        assert.equal(message, "6th", "got exception msg");
        break;
      default:
        assert.fail("Got unexception message: " + i);
    }
  }

  let { loader, messages } = LoaderWithHookedConsole(module, onMessage);
  let console = loader.globals.console;
  console.log("1st");
  console.error("2nd");
  console.warn("3rd");
  console.info("4th");
  console.debug("5th");
  console.exception("6th");
  assert.equal(messages.length, 6, "Got all console messages");
  assert.deepEqual(messages[0], {type: "log", msg: "1st"}, "Got log");
  assert.deepEqual(messages[1], {type: "error", msg: "2nd"}, "Got error");
  assert.deepEqual(messages[2], {type: "warn", msg: "3rd"}, "Got warn");
  assert.deepEqual(messages[3], {type: "info", msg: "4th"}, "Got info");
  assert.deepEqual(messages[4], {type: "debug", msg: "5th"}, "Got debug");
  assert.deepEqual(messages[5], {type: "exception", msg: "6th"}, "Got exception");
  assert.equal(count, 6, "Called for all messages");
};

require("sdk/test").run(exports);
