/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-return-values.html");
  await togglePauseOnExceptions(dbg, true, true);

  info("Test return values");
  await testReturnValue(dbg, "to sender", [
    { label: "return_something" },
    { label: "<return>", value: uneval("to sender") },
  ]);
  await testReturnValue(dbg, "undefined", [
    { label: "return_something" },
    { label: "<return>", value: '"undefined"' },
  ]);
  await testReturnValue(dbg, 2, [
    { label: "return_something" },
    { label: "<return>", value: "2" },
  ]);
  await testReturnValue(dbg, new Error("blah"), [
    { label: "return_something" },
    { label: "<return>", value: "Restricted" },
  ]);

  info("Test throw values");
  await testThrowValue(dbg, "a fit", [
    { label: "callee" },
    // The "uneval" call here gives us the way one would write `val` as
    // a JavaScript expression, similar to the way the debugger
    // displays values, so this test works when `val` is a string.
    { label: "<exception>", value: uneval("a fit") },
  ]);
  await testThrowValue(dbg, "undefined", [
    { label: "callee" },
    { label: "<exception>", value: '"undefined"' },
  ]);
  await testThrowValue(dbg, 2, [
    { label: "callee" },
    { label: "<exception>", value: "2" },
  ]);
  await testThrowValue(dbg, new Error("blah"), [
    { label: "callee" },
    { label: "<exception>", value: "Restricted" },
  ]);
});

async function testReturnValue(dbg, val, expectedScopeNodes) {
  invokeInTab("return_something", val);
  await waitForPaused(dbg);

  // "Step in" 2 times to get to the point where the debugger can
  // see the return value.
  await stepIn(dbg);
  await stepIn(dbg);

  for (const [i, scopeNode] of expectedScopeNodes.entries()) {
    const index = i + 1;
    is(
      getScopeNodeLabel(dbg, index),
      scopeNode.label,
      `check for ${scopeNode.label}`
    );
    if (scopeNode.value) {
      is(
        getScopeNodeValue(dbg, index),
        scopeNode.value,
        `check value is ${scopeNode.value}`
      );
    }
  }

  await resume(dbg);
  assertNotPaused(dbg);
}

async function testThrowValue(dbg, val, expectedScopeNodes) {
  invokeInTab("throw_something", val);
  await waitForPaused(dbg);

  // "Step in" once to get to the point where the debugger can see the
  // exception.
  await stepIn(dbg);

  for (const [i, scopeNode] of expectedScopeNodes.entries()) {
    const index = i + 1;
    is(
      getScopeNodeLabel(dbg, index),
      scopeNode.label,
      `check for ${scopeNode.label}`
    );
    if (scopeNode.value) {
      is(
        getScopeNodeValue(dbg, index),
        scopeNode.value,
        `check exception is ${scopeNode.value}`
      );
    }
  }

  await resume(dbg);
  assertNotPaused(dbg);
}
