/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

async function run_test() {
  try {
    do_test_pending();
    await run_test_with_server(DebuggerServer);
    await run_test_with_server(WorkerDebuggerServer);
  } finally {
    do_test_finished();
  }
}

async function run_test_with_server(server) {
  initTestDebuggerServer(server);
  const debuggee = addTestGlobal("test-grips", server);
  debuggee.eval(`
    function stopMe(arg1) {
      debugger;
    }
  `);

  const dbgClient = new DebuggerClient(server.connectPipe());
  await dbgClient.connect();
  const [,, threadClient] = await attachTestTabAndResume(dbgClient, "test-grips");

  await test_object_grip(debuggee, threadClient);

  await dbgClient.close();
}

async function test_object_grip(debuggee, threadClient) {
  await assert_object_argument(
    debuggee,
    threadClient,
    `
      var obj = {
        stringProp: "a value",
        get stringNormal(){
          return "a value";
        },
        get stringAbrupt() {
          throw "a value";
        },
        get objectNormal() {
          return { prop: 4 };
        },
        get objectAbrupt() {
          throw { prop: 4 };
        },
        get context(){
          return this === obj ? "correct context" : "wrong context";
        },
        method() {
          return "a value";
        },
      };
      stopMe(obj);
    `,
    async objClient => {
      const expectedValues = {
        stringProp: {
          return: "a value",
        },
        stringNormal: {
          return: "a value",
        },
        stringAbrupt: {
          throw: "a value",
        },
        objectNormal: {
          return: {
            type: "object",
            class: "Object",
            ownPropertyLength: 1,
            preview: {
              kind: "Object",
              ownProperties: {
                prop: {
                  value: 4,
                },
              },
            },
          },
        },
        objectAbrupt: {
          throw: {
            type: "object",
            class: "Object",
            ownPropertyLength: 1,
            preview: {
              kind: "Object",
              ownProperties: {
                prop: {
                  value: 4,
                },
              },
            },
          },
        },
        context: {
          return: "correct context",
        },
        method: {
          return: {
            type: "object",
            class: "Function",
            name: "method",
          },
        },
      };

      for (const [key, expected] of Object.entries(expectedValues)) {
        const { value } = await objClient.getPropertyValue(key);

        assert_completion(value, expected);
      }
    },
  );
}

function assert_object_argument(debuggee, threadClient, code, objectHandler) {
  return new Promise((resolve, reject) => {
    threadClient.addOneTimeListener("paused", function(event, packet) {
      (async () => {
        try {
          const arg1 = packet.frame.arguments[0];
          Assert.equal(arg1.class, "Object");

          await objectHandler(threadClient.pauseGrip(arg1));
        } finally {
          await threadClient.resume();
        }
      })().then(resolve, reject);
    });

    // This synchronously blocks until 'threadClient.resume()' above runs
    // because the 'paused' event runs everthing in a new event loop.
    debuggee.eval(code);
  });
}

function assert_completion(value, expected) {
  if (expected && "return" in expected) {
    assert_value(value.return, expected.return);
  }
  if (expected && "throw" in expected) {
    assert_value(value.throw, expected.throw);
  }
  if (!expected) {
    assert_value(value, expected);
  }
}

function assert_value(actual, expected) {
  Assert.equal(typeof actual, typeof expected);

  if (typeof expected === "object") {
    // Note: We aren't using deepEqual here because we're only doing a cursory
    // check of a few properties, not a full comparison of the result, since
    // the full outputs includes stuff like preview info that we don't need.
    for (const key of Object.keys(expected)) {
      assert_value(actual[key], expected[key]);
    }
  } else {
    Assert.equal(actual, expected);
  }
}
