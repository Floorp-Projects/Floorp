/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

Cu.importGlobalProperties(["ChromeUtils"]);

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
  equal(frame.column, 14, "Frame column");
  equal(frame.functionDisplayName, "it", "Frame function name");
  equal(frame.parent, null, "Frame parent");

  equal(String(frame), "it@thing.js:5:14\n", "Stringified frame");
});
