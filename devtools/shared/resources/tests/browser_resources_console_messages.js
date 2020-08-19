/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around CONSOLE_MESSAGE
//
// Reproduces assertions from: devtools/shared/webconsole/test/chrome/test_cached_messages.html

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

add_task(async function() {
  await testConsoleMessagesResources();
  await testConsoleMessagesResourcesWithIgnoreExistingResources();
});

async function testConsoleMessagesResources() {
  const tab = await addTab("data:text/html,Console Messages");

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  info(
    "Log some messages *before* calling ResourceWatcher.watchResources in order to " +
      "assert the behavior of already existing messages."
  );
  await logExistingMessages(tab.linkedBrowser);

  let runtimeDoneResolve;
  const expectedExistingCalls = [...expectedExistingConsoleCalls];
  const expectedRuntimeCalls = [...expectedRuntimeConsoleCalls];
  const onRuntimeDone = new Promise(resolve => (runtimeDoneResolve = resolve));
  const onAvailable = ({ resourceType, targetFront, resource }) => {
    is(
      resourceType,
      ResourceWatcher.TYPES.CONSOLE_MESSAGE,
      "Received a message"
    );
    ok(resource.message, "message is wrapped into a message attribute");
    const expected = (expectedExistingCalls.length > 0
      ? expectedExistingCalls
      : expectedRuntimeCalls
    ).shift();
    checkConsoleAPICall(resource.message, expected);
    if (expectedRuntimeCalls.length == 0) {
      runtimeDoneResolve();
    }
  };

  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable,
    }
  );
  is(
    expectedExistingCalls.length,
    0,
    "Got the expected number of existing messages"
  );

  info(
    "Now log messages *after* the call to ResourceWatcher.watchResources and after having received all existing messages"
  );
  await logRuntimeMessages(tab.linkedBrowser);

  info("Waiting for all runtime messages");
  await onRuntimeDone;

  is(
    expectedRuntimeCalls.length,
    0,
    "Got the expected number of runtime messages"
  );

  targetList.stopListening();
  await client.close();
}

async function testConsoleMessagesResourcesWithIgnoreExistingResources() {
  info("Test ignoreExistingResources option for console messages");
  const tab = await addTab("data:text/html,Console Messages");

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  info(
    "Check whether onAvailable will not be called with existing console messages"
  );
  await logExistingMessages(tab.linkedBrowser);

  const availableResources = [];
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: ({ resource }) => availableResources.push(resource),
      ignoreExistingResources: true,
    }
  );
  is(
    availableResources.length,
    0,
    "onAvailable wasn't called for existing console messages"
  );

  info(
    "Check whether onAvailable will be called with the future console messages"
  );
  await logRuntimeMessages(tab.linkedBrowser);
  await waitUntil(
    () => availableResources.length === expectedRuntimeConsoleCalls.length
  );
  for (let i = 0; i < expectedRuntimeConsoleCalls.length; i++) {
    const { message, targetFront } = availableResources[i];
    is(
      targetFront,
      targetList.targetFront,
      "The targetFront property is the expected one"
    );
    const expected = expectedRuntimeConsoleCalls[i];
    checkConsoleAPICall(message, expected);
  }

  await targetList.stopListening();
  await client.close();
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

const NUMBER_REGEX = /^\d+$/;

const defaultStackFrames = [
  {
    filename: "resource://testing-common/content-task.js",
    lineNumber: NUMBER_REGEX,
    columnNumber: NUMBER_REGEX,
  },
  {
    filename: "resource://testing-common/content-task.js",
    lineNumber: NUMBER_REGEX,
    columnNumber: NUMBER_REGEX,
    asyncCause: "MessageListener.receiveMessage",
  },
];

const expectedExistingConsoleCalls = [
  {
    level: "log",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: NUMBER_REGEX,
    arguments: ["foobarBaz-log", { type: "undefined" }],
  },
  {
    level: "info",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: NUMBER_REGEX,
    arguments: ["foobarBaz-info", { type: "null" }],
  },
  {
    level: "warn",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: NUMBER_REGEX,
    arguments: ["foobarBaz-warn", { type: "object", actor: /[a-z]/ }],
  },
];

const longString = new Array(DevToolsServer.LONG_STRING_LENGTH + 2).join("a");
const expectedRuntimeConsoleCalls = [
  {
    level: "log",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: NUMBER_REGEX,
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
    timeStamp: NUMBER_REGEX,
    arguments: ["foobarBaz-info", { type: "null" }],
  },
  {
    level: "warn",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: NUMBER_REGEX,
    arguments: ["foobarBaz-warn", { type: "object", actor: /[a-z]/ }],
  },
  {
    level: "debug",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: NUMBER_REGEX,
    arguments: [{ type: "null" }],
  },
  {
    level: "trace",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: NUMBER_REGEX,
    stacktrace: [
      {
        filename: EXPECTED_FILENAME,
        functionName: EXPECTED_FUNCTION_NAME,
      },
      ...defaultStackFrames,
    ],
  },
  {
    level: "dir",
    filename: EXPECTED_FILENAME,
    functionName: EXPECTED_FUNCTION_NAME,
    timeStamp: NUMBER_REGEX,
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
    timeStamp: NUMBER_REGEX,
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
    timeStamp: NUMBER_REGEX,
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
    timeStamp: NUMBER_REGEX,
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
      ...defaultStackFrames,
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
  is(
    call.arguments?.length || 0,
    expected.arguments?.length || 0,
    "number of arguments"
  );

  checkObject(call, expected);
}
