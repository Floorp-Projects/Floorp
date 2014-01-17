/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 "use strict";

var traceback = require("sdk/console/traceback");
var {Cc,Ci,Cr,Cu} = require("chrome");
const { on, off } = require("sdk/system/events");

function throwNsIException() {
  var ios = Cc['@mozilla.org/network/io-service;1']
            .getService(Ci.nsIIOService);
  ios.newURI("i'm a malformed URI", null, null);
}

function throwError() {
  throw new Error("foob");
}

exports.testFormatDoesNotFetchRemoteFiles = function(assert) {
  ["http", "https"].forEach(
    function(scheme) {
      var httpRequests = 0;
      function onHttp() {
        httpRequests++;
      }

      on("http-on-modify-request", onHttp);

      try {
        var tb = [{filename: scheme + "://www.mozilla.org/",
                   lineNumber: 1,
                   name: "blah"}];
        traceback.format(tb);
      } catch (e) {
        assert.fail(e);
      }

      off("http-on-modify-request", onHttp);

      assert.equal(httpRequests, 0,
                       "traceback.format() does not make " +
                       scheme + " request");
    });
};

exports.testFromExceptionWithString = function(assert) {
  try {
    throw "foob";
    assert.fail("an exception should've been thrown");
  } catch (e if e == "foob") {
    var tb = traceback.fromException(e);
    assert.equal(tb.length, 0);
  }
};

exports.testFormatWithString = function(assert) {
  // This can happen if e.g. a thrown exception was
  // a string instead of an Error instance.
  assert.equal(traceback.format("blah"),
		   "Traceback (most recent call last):");
};

exports.testFromExceptionWithError = function(assert) {
  try {
    throwError();
    assert.fail("an exception should've been thrown");
  } catch (e if e instanceof Error) {
    var tb = traceback.fromException(e);

    var xulApp = require("sdk/system/xul-app");
    assert.equal(tb.slice(-1)[0].name, "throwError");
  }
};

exports.testFromExceptionWithNsIException = function(assert) {
  try {
    throwNsIException();
    assert.fail("an exception should've been thrown");
  } catch (e if e.result == Cr.NS_ERROR_MALFORMED_URI) {
    var tb = traceback.fromException(e);
    assert.equal(tb[tb.length - 1].name, "throwNsIException");
  }
};

exports.testFormat = function(assert) {
  function getTraceback() {
    return traceback.format();
  }

  var formatted = getTraceback();
  assert.equal(typeof(formatted), "string");
  var lines = formatted.split("\n");

  assert.equal(lines[lines.length - 2].indexOf("getTraceback") > 0,
                   true,
                   "formatted traceback should include function name");

  assert.equal(lines[lines.length - 1].trim(),
                   "return traceback.format();",
                   "formatted traceback should include source code");
};

exports.testExceptionsWithEmptyStacksAreLogged = function(assert) {
  // Ensures that our fix to bug 550368 works.
  var sandbox = Cu.Sandbox("http://www.foo.com");
  var excRaised = false;
  try {
    Cu.evalInSandbox("returns 1 + 2;", sandbox, "1.8",
                     "blah.js", 25);
  } catch (e) {
    excRaised = true;
    var stack = traceback.fromException(e);
    assert.equal(stack.length, 1, "stack should have one frame");

    assert.ok(stack[0].fileName, "blah.js", "frame should have filename");
    assert.ok(stack[0].lineNumber, 25, "frame should have line no");
    assert.equal(stack[0].name, null, "frame should have null function name");
  }
  if (!excRaised)
    assert.fail("Exception should have been raised.");
};

require('sdk/test').run(exports);
