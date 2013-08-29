/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const errors = require("sdk/deprecated/errors");

exports.testCatchAndLog = function(assert) {
  var caught = [];
  function dummyLog(e) { caught.push(e); }

  var wrapped = errors.catchAndLog(function(x) {
    throw Error("blah" + x + this);
  }, "boop", dummyLog);

  assert.equal(wrapped.call("hi", 1), "boop",
               "exceptions should be trapped, def. resp. returned");
  assert.equal(caught.length, 1,
               "logging function should be called");
  assert.equal(caught[0].message, "blah1hi",
               "args and this should be passed to wrapped func");
};

exports.testCatchAndLogProps = function(assert) {
  var caught = [];
  function dummyLog(e) { caught.push(e); }

  var thing = {
    foo: function(x) { throw Error("nowai" + x); },
    bar: function() { throw Error("blah"); },
    baz: function() { throw Error("fnarg"); }
  };

  errors.catchAndLogProps(thing, "foo", "ugh", dummyLog);

  assert.equal(thing.foo(1), "ugh",
                   "props should be wrapped");
  assert.equal(caught.length, 1,
                   "logging function should be called");
  assert.equal(caught[0].message, "nowai1",
                   "args should be passed to wrapped func");
  assert.throws(function() { thing.bar(); },
                /blah/,
                "non-wrapped props should be wrapped");

  errors.catchAndLogProps(thing, ["bar", "baz"], "err", dummyLog);
  assert.ok((thing.bar() == thing.baz()) &&
              (thing.bar() == "err"),
              "multiple props should be wrapped if array passed in");
};

exports.testCatchAndReturn = function(assert) {
  var wrapped = errors.catchAndReturn(function(x) {
    if (x == 1)
      return "one";
    if (x == 2)
      throw new Error("two");
    return this + x;
  });

  assert.equal(wrapped(1).returnValue, "one",
               "arg should be passed; return value should be returned");
  assert.ok(wrapped(2).exception, "exception should be returned");
  assert.equal(wrapped(2).exception.message, "two", "message is correct");
  assert.ok(wrapped(2).exception.fileName.indexOf("test-errors.js") != -1,
            "filename is present");
  assert.ok(wrapped(2).exception.stack, "stack is available");
  assert.equal(wrapped.call("hi", 3).returnValue, "hi3",
               "`this` should be set correctly");
};

require("sdk/test").run(exports);
