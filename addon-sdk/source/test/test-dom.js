/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const events = require("sdk/dom/events");
const { activeBrowserWindow: { document } } = require("sdk/deprecated/window-utils");
const window = document.window;
/*
exports["test on / emit"] = function (assert, done) {
  let element = document.createElement("div");
  events.on(element, "click", function listener(event) {
    assert.equal(event.target, element, "event has correct target");
    events.removeListener(element, "click", listener);
    done();
  });

  events.emit(element, "click", {
    category: "MouseEvents",
    settings: [
      true, true, window, 0, 0, 0, 0, 0, false, false, false, false, 0, null
    ]
  });
};

exports["test remove"] = function (assert, done) {
  let element = document.createElement("span");
  let l1 = 0;
  let l2 = 0;
  let options = {
    category: "MouseEvents",
    settings: [
      true, true, window, 0, 0, 0, 0, 0, false, false, false, false, 0, null
    ]
  };

  events.on(element, "click", function listener1(event) {
    l1 ++;
    assert.equal(event.target, element, "event has correct target");
    events.removeListener(element, "click", listener1);
  });

  events.on(element, "click", function listener2(event) {
    l2 ++;
    if (l1 < l2) {
      assert.equal(l1, 1, "firs listener was called and then romeved");
      events.removeListener(element, "click", listener2);
      done();
    }
    events.emit(element, "click", options);
  });

  events.emit(element, "click", options);
};

exports["test once"] = function (assert, done) {
  let element = document.createElement("h1");
  let l1 = 0;
  let l2 = 0;
  let options = {
    category: "MouseEvents",
    settings: [
      true, true, window, 0, 0, 0, 0, 0, false, false, false, false, 0, null
    ]
  };


  events.once(element, "click", function listener(event) {
    assert.equal(event.target, element, "event target is a correct element");
    l1 ++;
  });

  events.on(element, "click", function listener(event) {
    l2 ++;
    if (l2 > 3) {
      events.removeListener(element, "click", listener);
      assert.equal(event.target, element, "event has correct target");
      assert.equal(l1, 1, "once was called only once");
      done();
    }
    events.emit(element, "click", options);
  });

  events.emit(element, "click", options);
}
*/
require("test").run(exports);
