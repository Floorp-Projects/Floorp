/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable max-nested-callbacks */

"use strict";

// Test getDisplayString.

Cu.import("resource://testing-common/PromiseTestUtils.jsm", this);

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  gDebuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-grips",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_display_string();
                           });
  });
  do_test_pending();
}

function test_display_string() {
  const testCases = [
    {
      input: "new Boolean(true)",
      output: "true"
    },
    {
      input: "new Number(5)",
      output: "5"
    },
    {
      input: "new String('foo')",
      output: "foo"
    },
    {
      input: "new Map()",
      output: "[object Map]"
    },
    {
      input: "[,,,,,,,]",
      output: ",,,,,,"
    },
    {
      input: "[1, 2, 3]",
      output: "1,2,3"
    },
    {
      input: "[undefined, null, true, 'foo', 5]",
      output: ",,true,foo,5"
    },
    {
      input: "[{},{}]",
      output: "[object Object],[object Object]"
    },
    {
      input: "(" + function () {
        const arr = [1];
        arr.push(arr);
        return arr;
      } + ")()",
      output: "1,"
    },
    {
      input: "{}",
      output: "[object Object]"
    },
    {
      input: "Object.create(null)",
      output: "[object Object]"
    },
    {
      input: "new Error('foo')",
      output: "Error: foo"
    },
    {
      input: "new SyntaxError()",
      output: "SyntaxError"
    },
    {
      input: "new ReferenceError('')",
      output: "ReferenceError"
    },
    {
      input: "(" + function () {
        const err = new Error("bar");
        err.name = "foo";
        return err;
      } + ")()",
      output: "foo: bar"
    },
    {
      input: "() => {}",
      output: "() => {}"
    },
    {
      input: "function (foo, bar) {}",
      output: "function (foo, bar) {}"
    },
    {
      input: "function foo(bar) {}",
      output: "function foo(bar) {}"
    },
    {
      input: "Array",
      output: Array + ""
    },
    {
      input: "/foo[bar]/g",
      output: "/foo[bar]/g"
    },
    {
      input: "new Proxy({}, {})",
      output: "[object Object]"
    },
    {
      input: "Promise.resolve(5)",
      output: "Promise (fulfilled: 5)"
    },
    {
      // This rejection is left uncaught, see expectUncaughtRejection below.
      input: "Promise.reject(new Error())",
      output: "Promise (rejected: Error)"
    },
    {
      input: "new Promise(function () {})",
      output: "Promise (pending)"
    }
  ];

  PromiseTestUtils.expectUncaughtRejection(/Error/);

  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    const args = packet.frame.arguments;

    (function loop() {
      const objClient = gThreadClient.pauseGrip(args.pop());
      objClient.getDisplayString(function ({ displayString }) {
        do_check_eq(displayString, testCases.pop().output);
        if (args.length) {
          loop();
        } else {
          gThreadClient.resume(function () {
            finishClient(gClient);
          });
        }
      });
    })();
  });

  const inputs = testCases.map(({ input }) => input).join(",");
  gDebuggee.eval("stopMe(" + inputs + ")");
}
