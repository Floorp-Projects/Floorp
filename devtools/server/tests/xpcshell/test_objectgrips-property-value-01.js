/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const arg1 = packet.frame.arguments[0];
    Assert.equal(arg1.class, "Object");

    const objFront = threadFront.pauseGrip(arg1);

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
          _grip: {
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
      },
      objectAbrupt: {
        throw: {
          _grip: {
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
      },
      context: {
        return: "correct context",
      },
      method: {
        return: {
          _grip: {
            type: "object",
            class: "Function",
            name: "method",
          },
        },
      },
    };

    for (const [key, expected] of Object.entries(expectedValues)) {
      const { value } = await objFront.getPropertyValue(key, null);
      assert_completion(value, expected);
    }

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    function stopMe(arg1) {
      debugger;
    }.toString()
  );

  debuggee.eval(`
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
  `);
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
