/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable max-nested-callbacks */

"use strict";

// Test getDisplayString.

const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);

var gDebuggee;
var gThreadFront;

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    gThreadFront = threadFront;
    gDebuggee = debuggee;
    await test_display_string();
  })
);

async function test_display_string() {
  const testCases = [
    {
      input: "new Boolean(true)",
      output: "true",
    },
    {
      input: "new Number(5)",
      output: "5",
    },
    {
      input: "new String('foo')",
      output: "foo",
    },
    {
      input: "new Map()",
      output: "[object Map]",
    },
    {
      input: "[,,,,,,,]",
      output: ",,,,,,",
    },
    {
      input: "[1, 2, 3]",
      output: "1,2,3",
    },
    {
      input: "[undefined, null, true, 'foo', 5]",
      output: ",,true,foo,5",
    },
    {
      input: "[{},{}]",
      output: "[object Object],[object Object]",
    },
    {
      input:
        "(" +
        function() {
          const arr = [1];
          arr.push(arr);
          return arr;
        } +
        ")()",
      output: "1,",
    },
    {
      input: "{}",
      output: "[object Object]",
    },
    {
      input: "Object.create(null)",
      output: "[object Object]",
    },
    {
      input: "new Error('foo')",
      output: "Error: foo",
    },
    {
      input: "new SyntaxError()",
      output: "SyntaxError",
    },
    {
      input: "new ReferenceError('')",
      output: "ReferenceError",
    },
    {
      input:
        "(" +
        function() {
          const err = new Error("bar");
          err.name = "foo";
          return err;
        } +
        ")()",
      output: "foo: bar",
    },
    {
      input: "() => {}",
      output: "() => {}",
    },
    {
      input: "function (foo, bar) {}",
      output: "function (foo, bar) {}",
    },
    {
      input: "function foo(bar) {}",
      output: "function foo(bar) {}",
    },
    {
      input: "Array",
      output: Array + "",
    },
    {
      input: "/foo[bar]/g",
      output: "/foo[bar]/g",
    },
    {
      input: "new Proxy({}, {})",
      output: "<proxy>",
    },
    {
      input: "Promise.resolve(5)",
      output: "Promise (fulfilled: 5)",
    },
    {
      // This rejection is left uncaught, see expectUncaughtRejection below.
      input: "Promise.reject(new Error())",
      output: "Promise (rejected: Error)",
    },
    {
      input: "new Promise(function () {})",
      output: "Promise (pending)",
    },
  ];

  PromiseTestUtils.expectUncaughtRejection(/Error/);

  const inputs = testCases.map(({ input }) => input).join(",");

  const packet = await executeOnNextTickAndWaitForPause(
    () => evalCode(inputs),
    gThreadFront
  );

  const args = packet.frame.arguments;

  while (args.length) {
    const objClient = gThreadFront.pauseGrip(args.pop());
    const response = await objClient.getDisplayString();
    Assert.equal(response.displayString, testCases.pop().output);
  }

  await gThreadFront.resume();
}

function evalCode(inputs) {
  gDebuggee.eval(
    function stopMe(arg1) {
      debugger;
    }.toString()
  );

  gDebuggee.eval("stopMe(" + inputs + ")");
}
