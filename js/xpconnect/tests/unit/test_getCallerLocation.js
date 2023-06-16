/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

Cu.importGlobalProperties(["ChromeUtils"]);

const {AddonTestUtils} = ChromeUtils.importESModule("resource://testing-common/AddonTestUtils.sys.mjs");

add_task(async function() {
  const sandbox = Cu.Sandbox("http://example.com/");

  function foo() {
    return bar();
  }

  function bar() {
    return baz();
  }

  function baz() {
    return ChromeUtils.getCallerLocation(Cu.getObjectPrincipal(sandbox));
  }

  Cu.evalInSandbox(`
    function it() {
      // Use map() to throw a self-hosted frame on the stack, which we
      // should filter out.
      return [0].map(foo)[0];
    }
    function thing() {
      return it();
    }
  `, sandbox, undefined, "thing.js");

  Cu.exportFunction(foo, sandbox, {defineAs: "foo"});

  let frame = sandbox.thing();

  equal(frame.source, "thing.js", "Frame source");
  equal(frame.line, 5, "Frame line");
  equal(frame.column, 18, "Frame column");
  equal(frame.functionDisplayName, "it", "Frame function name");
  equal(frame.parent, null, "Frame parent");

  equal(String(frame), "it@thing.js:5:18\n", "Stringified frame");


  // reportError

  let {messages} = await AddonTestUtils.promiseConsoleOutput(() => {
    Cu.reportError("Meh", frame);
  });

  let [msg] = messages.filter(m => m.message.includes("Meh"));

  equal(msg.stack, frame, "reportError stack frame");
  equal(msg.message, '[JavaScript Error: "Meh" {file: "thing.js" line: 5}]\nit@thing.js:5:18\n');

  Assert.throws(() => { Cu.reportError("Meh", {}); },
                err => err.result == Cr.NS_ERROR_INVALID_ARG,
                "reportError should throw when passed a non-SavedFrame object");


  // createError

  Assert.throws(() => { ChromeUtils.createError("Meh", {}); },
                err => err.result == Cr.NS_ERROR_INVALID_ARG,
                "createError should throw when passed a non-SavedFrame object");

  let cloned = Cu.cloneInto(frame, sandbox);
  let error = ChromeUtils.createError("Meh", cloned);

  equal(String(cloned), String(frame),
        "Cloning a SavedStack preserves its stringification");

  equal(Cu.getGlobalForObject(error), sandbox,
        "createError creates errors in the global of the SavedFrame");
  equal(error.stack, String(cloned),
        "createError creates errors with the correct stack");

  equal(error.message, "Meh", "Error message");
  equal(error.fileName, "thing.js", "Error filename");
  equal(error.lineNumber, 5, "Error line");
  equal(error.columnNumber, 18, "Error column");
});
