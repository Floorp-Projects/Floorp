/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around CONSOLE_MESSAGES
//
// Reproduces assertions from: devtools/shared/webconsole/test/chrome/test_cached_messages.html

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

add_task(async function() {
  // Open a test tab
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab("data:text/html,Console Messages");

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  await testMessages(tab.linkedBrowser, resourceWatcher);

  targetList.stopListening();
  await client.close();
});

async function testMessages(browser, resourceWatcher) {
  info(
    "Log some messages *before* calling ResourceWatcher.watch in order to assert the behavior of already existing messages."
  );
  await logExistingMessages(browser);

  let runtimeDoneResolve;
  const onRuntimeDone = new Promise(resolve => (runtimeDoneResolve = resolve));
  await resourceWatcher.watch(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGES],
    ({ resourceType, targetFront, resource }) => {
      is(
        resourceType,
        ResourceWatcher.TYPES.CONSOLE_MESSAGES,
        "Received a message"
      );
      ok(resource.message, "message is wrapped into a message attribute");
      const expected = (expectedExistingConsoleCalls.length > 0
        ? expectedExistingConsoleCalls
        : expectedRuntimeConsoleCalls
      ).shift();
      checkConsoleAPICall(resource.message, expected);
      if (expectedRuntimeConsoleCalls.length == 0) {
        runtimeDoneResolve();
      }
    }
  );
  is(
    expectedExistingConsoleCalls.length,
    0,
    "Got the expected number of existing messages"
  );

  info(
    "Now log messages *after* the call to ResourceWatcher.watch and after having received all existing messages"
  );
  await logRuntimeMessages(browser);

  info("Waiting for all runtime messages");
  await onRuntimeDone;

  is(
    expectedRuntimeConsoleCalls.length,
    0,
    "Got the expected number of runtime messages"
  );
}

// For both existing and runtime messages, we execute console API
// from a frame script evaluated via ContentTask.
// Records here the filename used by ContentTask and the function
// name of the function passed to ContentTask.spawn.
const EXPECTED_FILENAME = /content-task.js/;
const EXPECTED_FUNCTION_NAME = "frameScript";

function logExistingMessages(browser) {
  return ContentTask.spawn(browser, null, function frameScript() {
    content.console.log("foobarBaz-log", undefined);
    content.console.info("foobarBaz-info", null);
    content.console.warn("foobarBaz-warn", content.document.body);
  });
}
const expectedExistingConsoleCalls = [
  {
    _type: "ConsoleAPI",
    level: "log",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: /^\d+$/,
    arguments: ["foobarBaz-log", { type: "undefined" }],
  },
  {
    _type: "ConsoleAPI",
    level: "info",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: /^\d+$/,
    arguments: ["foobarBaz-info", { type: "null" }],
  },
  {
    _type: "ConsoleAPI",
    level: "warn",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: /^\d+$/,
    arguments: ["foobarBaz-warn", { type: "object", actor: /[a-z]/ }],
  },
];

const longString = new Array(DevToolsServer.LONG_STRING_LENGTH + 2).join("a");
const expectedRuntimeConsoleCalls = [
  {
    level: "log",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: /^\d+$/,
    arguments: ["foobarBaz-log", { type: "undefined" }],
  },
  {
    level: "log",
    arguments: ["Float from not a number: NaN"],
  },
  {
    level: "log",
    arguments: ["Float from string: 1.200000"],
  },
  {
    level: "log",
    arguments: ["Float from number: 1.300000"],
  },
  {
    level: "info",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: /^\d+$/,
    arguments: ["foobarBaz-info", { type: "null" }],
  },
  {
    level: "warn",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: /^\d+$/,
    arguments: ["foobarBaz-warn", { type: "object", actor: /[a-z]/ }],
  },
  {
    level: "debug",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: /^\d+$/,
    arguments: [{ type: "null" }],
  },
  {
    level: "trace",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: /^\d+$/,
    stacktrace: [
      {
        filename: EXPECTED_FILENAME,
        functionName: EXPECTED_FUNCTION_NAME,
      },
    ],
  },
  {
    level: "dir",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: /^\d+$/,
    arguments: [
      {
        type: "object",
        actor: /[a-z]/,
        class: "HTMLDocument",
      },
      {
        type: "object",
        actor: /[a-z]/,
        class: "Location",
      },
    ],
  },
  {
    level: "log",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: /^\d+$/,
    arguments: [
      "foo",
      {
        type: "longString",
        initial: longString.substring(
          0,
          DevToolsServer.LONG_STRING_INITIAL_LENGTH
        ),
        length: longString.length,
        actor: /[a-z]/,
      },
    ],
  },
  {
    level: "log",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: /^\d+$/,
    arguments: [
      {
        type: "object",
        actor: /[a-z]/,
        class: "Restricted",
      },
    ],
  },
  {
    level: "error",
    filename: EXPECTED_FILENAME,
    functionName: "fromAsmJS",
    timeStamp: /^\d+$/,
    arguments: ["foobarBaz-asmjs-error", { type: "undefined" }],

    stacktrace: [
      {
        filename: EXPECTED_FILENAME,
        functionName: "fromAsmJS",
      },
      {
        filename: EXPECTED_FILENAME,
        functionName: "inAsmJS2",
      },
      {
        filename: EXPECTED_FILENAME,
        functionName: "inAsmJS1",
      },
      {
        filename: EXPECTED_FILENAME,
        functionName: EXPECTED_FUNCTION_NAME,
      },
    ],
  },
];

function logRuntimeMessages(browser) {
  return ContentTask.spawn(browser, [], function frameScript() {
    // Re-create the longString copy in order to avoid having any wrapper
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );
    const { DevToolsServer } = require("devtools/server/devtools-server");
    const _longString = new Array(DevToolsServer.LONG_STRING_LENGTH + 2).join(
      "a"
    );

    content.console.log("foobarBaz-log", undefined);

    content.console.log("Float from not a number: %f", "foo");
    content.console.log("Float from string: %f", "1.2");
    content.console.log("Float from number: %f", 1.3);

    content.console.info("foobarBaz-info", null);
    content.console.warn("foobarBaz-warn", content.document.documentElement);
    content.console.debug(null);
    content.console.trace();
    content.console.dir(content.document, content.location);
    content.console.log("foo", _longString);

    const sandbox = new Cu.Sandbox(null, { invisibleToDebugger: true });
    const sandboxObj = sandbox.eval("new Object");
    content.console.log(sandboxObj);

    function fromAsmJS() {
      content.console.error("foobarBaz-asmjs-error", undefined);
    }

    (function(global, foreign) {
      "use asm";
      function inAsmJS2() {
        foreign.fromAsmJS();
      }
      function inAsmJS1() {
        inAsmJS2();
      }
      return inAsmJS1;
    })(null, { fromAsmJS: fromAsmJS })();
  });
}

// Copied from devtools/shared/webconsole/test/chrome/common.js
function checkConsoleAPICall(call, expected) {
  is(call.arguments.length, expected.arguments.length, "number of arguments");

  checkObject(call, expected);
}

function checkObject(object, expected) {
  if (object && object.getGrip) {
    object = object.getGrip();
  }

  for (const name of Object.keys(expected)) {
    const expectedValue = expected[name];
    const value = object[name];
    checkValue(name, value, expectedValue);
  }
}

function checkValue(name, value, expected) {
  if (expected === null) {
    ok(!value, "'" + name + "' is null");
  } else if (value === undefined) {
    ok(false, "'" + name + "' is undefined");
  } else if (value === null) {
    ok(false, "'" + name + "' is null");
  } else if (
    typeof expected == "string" ||
    typeof expected == "number" ||
    typeof expected == "boolean"
  ) {
    is(value, expected, "property '" + name + "'");
  } else if (expected instanceof RegExp) {
    ok(expected.test(value), name + ": " + expected + " matched " + value);
  } else if (Array.isArray(expected)) {
    info("checking array for property '" + name + "'");
    checkObject(value, expected);
  } else if (typeof expected == "object") {
    info("checking object for property '" + name + "'");
    checkObject(value, expected);
  }
}
