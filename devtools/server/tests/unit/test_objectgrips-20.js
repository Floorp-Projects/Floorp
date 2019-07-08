/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

// Test that onEnumProperties returns the expected data
// when passing `ignoreNonIndexedProperties` and `ignoreIndexedProperties` options
// with various objects. (See Bug 1403065)

const DO_NOT_CHECK_VALUE = Symbol();

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(
  threadClientTest(async ({ threadClient, debuggee, client }) => {
    debuggee.eval(
      function stopMe(arg1) {
        debugger;
      }.toString()
    );

    const testCases = [
      {
        evaledObject: { a: 10 },
        expectedIndexedProperties: [],
        expectedNonIndexedProperties: [["a", 10]],
      },
      {
        evaledObject: { length: 10 },
        expectedIndexedProperties: [],
        expectedNonIndexedProperties: [["length", 10]],
      },
      {
        evaledObject: { a: 10, 0: "indexed" },
        expectedIndexedProperties: [["0", "indexed"]],
        expectedNonIndexedProperties: [["a", 10]],
      },
      {
        evaledObject: { 1: 1, length: 42, a: 10 },
        expectedIndexedProperties: [["1", 1]],
        expectedNonIndexedProperties: [["length", 42], ["a", 10]],
      },
      {
        evaledObject: { 1: 1, length: 2.34, a: 10 },
        expectedIndexedProperties: [["1", 1]],
        expectedNonIndexedProperties: [["length", 2.34], ["a", 10]],
      },
      {
        evaledObject: { 1: 1, length: -0, a: 10 },
        expectedIndexedProperties: [["1", 1]],
        expectedNonIndexedProperties: [["length", -0], ["a", 10]],
      },
      {
        evaledObject: { 1: 1, length: -10, a: 10 },
        expectedIndexedProperties: [["1", 1]],
        expectedNonIndexedProperties: [["length", -10], ["a", 10]],
      },
      {
        evaledObject: { 1: 1, length: true, a: 10 },
        expectedIndexedProperties: [["1", 1]],
        expectedNonIndexedProperties: [["length", true], ["a", 10]],
      },
      {
        evaledObject: { 1: 1, length: null, a: 10 },
        expectedIndexedProperties: [["1", 1]],
        expectedNonIndexedProperties: [
          ["length", DO_NOT_CHECK_VALUE],
          ["a", 10],
        ],
      },
      {
        evaledObject: { 1: 1, length: Math.pow(2, 53), a: 10 },
        expectedIndexedProperties: [["1", 1]],
        expectedNonIndexedProperties: [["length", 9007199254740992], ["a", 10]],
      },
      {
        evaledObject: { 1: 1, length: "fake", a: 10 },
        expectedIndexedProperties: [["1", 1]],
        expectedNonIndexedProperties: [["length", "fake"], ["a", 10]],
      },
      {
        evaledObject: { 1: 1, length: Infinity, a: 10 },
        expectedIndexedProperties: [["1", 1]],
        expectedNonIndexedProperties: [
          ["length", DO_NOT_CHECK_VALUE],
          ["a", 10],
        ],
      },
      {
        evaledObject: { 0: 0, length: 0 },
        expectedIndexedProperties: [["0", 0]],
        expectedNonIndexedProperties: [["length", 0]],
      },
      {
        evaledObject: { 0: 0, 1: 1, length: 1 },
        expectedIndexedProperties: [["0", 0], ["1", 1]],
        expectedNonIndexedProperties: [["length", 1]],
      },
      {
        evaledObject: { length: 0 },
        expectedIndexedProperties: [],
        expectedNonIndexedProperties: [["length", 0]],
      },
      {
        evaledObject: { 1: 1 },
        expectedIndexedProperties: [["1", 1]],
        expectedNonIndexedProperties: [],
      },
      {
        evaledObject: { a: 1, [2 ** 32 - 2]: 2, [2 ** 32 - 1]: 3 },
        expectedIndexedProperties: [["4294967294", 2]],
        expectedNonIndexedProperties: [["a", 1], ["4294967295", 3]],
      },
      {
        evaledObject: `(() => {
      x = [12, 42];
      x.foo = 90;
      return x;
    })()`,
        expectedIndexedProperties: [["0", 12], ["1", 42]],
        expectedNonIndexedProperties: [["length", 2], ["foo", 90]],
      },
      {
        evaledObject: `(() => {
      x = [12, 42];
      x.length = 3;
      return x;
    })()`,
        expectedIndexedProperties: [["0", 12], ["1", 42], ["2", undefined]],
        expectedNonIndexedProperties: [["length", 3]],
      },
      {
        evaledObject: `(() => {
      x = [12, 42];
      x.length = 1;
      return x;
    })()`,
        expectedIndexedProperties: [["0", 12]],
        expectedNonIndexedProperties: [["length", 1]],
      },
      {
        evaledObject: `(() => {
      x = [, 42,,];
      x.foo = 90;
      return x;
    })()`,
        expectedIndexedProperties: [
          ["0", undefined],
          ["1", 42],
          ["2", undefined],
        ],
        expectedNonIndexedProperties: [["length", 3], ["foo", 90]],
      },
      {
        evaledObject: `(() => {
      x = Array(2);
      x.foo = "bar";
      x.bar = "foo";
      return x;
    })()`,
        expectedIndexedProperties: [["0", undefined], ["1", undefined]],
        expectedNonIndexedProperties: [
          ["length", 2],
          ["foo", "bar"],
          ["bar", "foo"],
        ],
      },
      {
        evaledObject: `(() => {
      x = new Int8Array(new ArrayBuffer(2));
      x.foo = "bar";
      x.bar = "foo";
      return x;
    })()`,
        expectedIndexedProperties: [["0", 0], ["1", 0]],
        expectedNonIndexedProperties: [
          ["foo", "bar"],
          ["bar", "foo"],
          ["length", 2],
          ["buffer", DO_NOT_CHECK_VALUE],
          ["byteLength", 2],
          ["byteOffset", 0],
        ],
      },
    ];

    for (const test of testCases) {
      await test_object_grip(debuggee, client, threadClient, test);
    }
  })
);

// These tests are not yet supported in workers.
add_task(
  threadClientTest(
    async ({ threadClient, debuggee, client }) => {
      debuggee.eval(
        function stopMe(arg1) {
          debugger;
        }.toString()
      );

      const testCases = [
        {
          evaledObject: `(() => {
      x = new Int8Array([1, 2]);
      Object.defineProperty(x, 'length', {value: 0});
      return x;
    })()`,
          expectedIndexedProperties: [["0", 1], ["1", 2]],
          expectedNonIndexedProperties: [
            ["length", 0],
            ["buffer", DO_NOT_CHECK_VALUE],
            ["byteLength", 2],
            ["byteOffset", 0],
          ],
        },
        {
          evaledObject: `(() => {
      x = new Int32Array([1, 2]);
      Object.setPrototypeOf(x, null);
      return x;
    })()`,
          expectedIndexedProperties: [["0", 1], ["1", 2]],
          expectedNonIndexedProperties: [],
        },
      ];

      for (const test of testCases) {
        await test_object_grip(debuggee, client, threadClient, test);
      }
    },
    { doNotRunWorker: true }
  )
);

async function test_object_grip(
  debuggee,
  dbgClient,
  threadClient,
  testData = {}
) {
  const {
    evaledObject,
    expectedIndexedProperties,
    expectedNonIndexedProperties,
  } = testData;

  return new Promise((resolve, reject) => {
    threadClient.once("paused", async function(packet) {
      const [grip] = packet.frame.arguments;

      const objClient = threadClient.pauseGrip(grip);

      info(`
        Check enumProperties response for
        ${
          typeof evaledObject === "string"
            ? evaledObject
            : JSON.stringify(evaledObject)
        }
      `);

      // Checks the result of enumProperties.
      let response = await objClient.enumProperties({
        ignoreNonIndexedProperties: true,
      });
      await check_enum_properties(response, expectedIndexedProperties);

      response = await objClient.enumProperties({
        ignoreIndexedProperties: true,
      });
      await check_enum_properties(response, expectedNonIndexedProperties);

      await threadClient.resume();
      resolve();
    });

    // Be sure to run debuggee code in its own HTML 'task', so that when we call
    // the onDebuggerStatement hook, the test's own microtasks don't get suspended
    // along with the debuggee's.
    do_timeout(0, () => {
      debuggee.eval(`
        stopMe(${
          typeof evaledObject === "string"
            ? evaledObject
            : JSON.stringify(evaledObject)
        });
      `);
    });
  });
}

async function check_enum_properties(response, expected = []) {
  ok(
    response && Object.getOwnPropertyNames(response).includes("iterator"),
    "The response object has an iterator property"
  );

  const { iterator } = response;
  equal(
    iterator.count,
    expected.length,
    "iterator.count has the expected value"
  );

  info("Check iterator.slice response for all properties");
  const sliceResponse = await iterator.slice(0, iterator.count);
  ok(
    sliceResponse &&
      Object.getOwnPropertyNames(sliceResponse).includes("ownProperties"),
    "The response object has an ownProperties property"
  );

  const { ownProperties } = sliceResponse;
  const names = Object.getOwnPropertyNames(ownProperties);
  equal(
    names.length,
    expected.length,
    "The response has the expected number of properties"
  );
  for (let i = 0; i < names.length; i++) {
    const name = names[i];
    const [key, value] = expected[i];
    equal(name, key, "Property has the expected name");
    const property = ownProperties[name];

    if (value === DO_NOT_CHECK_VALUE) {
      return;
    }

    if (value === undefined) {
      equal(
        property,
        undefined,
        `Response has no value for the "${key}" property`
      );
    } else {
      const propValue = property.hasOwnProperty("value")
        ? property.value
        : property.getterValue;
      equal(propValue, value, `Property "${key}" has the expected value`);
    }
  }
}
