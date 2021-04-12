/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around CONSOLE_MESSAGE
//
// Reproduces assertions from: devtools/shared/webconsole/test/chrome/test_cached_messages.html
// And now more. Once we remove the console actor's startListeners in favor of watcher class
// We could remove that other old test.

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document.html";
const IFRAME_URL = URL_ROOT_ORG_SSL + "fission_iframe.html";

add_task(async function() {
  info("Execute test in top level document");
  await testTabConsoleMessagesResources(false);
  await testTabConsoleMessagesResourcesWithIgnoreExistingResources(false);

  info("Execute test in an iframe document, possibly remote with fission");
  await testTabConsoleMessagesResources(true);
  await testTabConsoleMessagesResourcesWithIgnoreExistingResources(true);
});

async function testTabConsoleMessagesResources(executeInIframe) {
  const tab = await addTab(FISSION_TEST_URL);

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  info(
    "Log some messages *before* calling ResourceWatcher.watchResources in order to " +
      "assert the behavior of already existing messages."
  );
  await logExistingMessages(tab.linkedBrowser, executeInIframe);

  const targetDocumentUrl = executeInIframe ? IFRAME_URL : FISSION_TEST_URL;

  let runtimeDoneResolve;
  const expectedExistingCalls = getExpectedExistingConsoleCalls(
    targetDocumentUrl
  );
  const expectedRuntimeCalls = getExpectedRuntimeConsoleCalls(
    targetDocumentUrl
  );
  const onRuntimeDone = new Promise(resolve => (runtimeDoneResolve = resolve));
  const onAvailable = resources => {
    for (const resource of resources) {
      if (resource.message.arguments?.[0] === "[WORKER] started") {
        // XXX Ignore message from workers as we can't know when they're logged, and we
        // have a dedicated test for them (browser_resources_console_messages_workers.js).
        continue;
      }

      is(
        resource.resourceType,
        ResourceWatcher.TYPES.CONSOLE_MESSAGE,
        "Received a message"
      );
      ok(resource.message, "message is wrapped into a message attribute");
      const isCachedMessage = expectedExistingCalls.length > 0;
      const expected = (isCachedMessage
        ? expectedExistingCalls
        : expectedRuntimeCalls
      ).shift();
      checkConsoleAPICall(resource.message, expected);
      is(
        resource.isAlreadyExistingResource,
        isCachedMessage,
        "isAlreadyExistingResource has the expected value"
      );

      if (expectedRuntimeCalls.length == 0) {
        runtimeDoneResolve();
      }
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
  await logRuntimeMessages(tab.linkedBrowser, executeInIframe);

  info("Waiting for all runtime messages");
  await onRuntimeDone;

  is(
    expectedRuntimeCalls.length,
    0,
    "Got the expected number of runtime messages"
  );

  targetCommand.destroy();
  await client.close();

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // registrationPromise is set by the test page.
    const registration = await content.wrappedJSObject.registrationPromise;
    registration.unregister();
  });
}

async function testTabConsoleMessagesResourcesWithIgnoreExistingResources(
  executeInIframe
) {
  info("Test ignoreExistingResources option for console messages");
  const tab = await addTab(FISSION_TEST_URL);

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  info(
    "Check whether onAvailable will not be called with existing console messages"
  );
  await logExistingMessages(tab.linkedBrowser, executeInIframe);

  const availableResources = [];
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: resources => availableResources.push(...resources),
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
  await logRuntimeMessages(tab.linkedBrowser, executeInIframe);
  const targetDocumentUrl = executeInIframe ? IFRAME_URL : FISSION_TEST_URL;
  const expectedRuntimeConsoleCalls = getExpectedRuntimeConsoleCalls(
    targetDocumentUrl
  );
  await waitUntil(
    () => availableResources.length === expectedRuntimeConsoleCalls.length
  );
  const expectedTargetFront =
    executeInIframe && isFissionEnabled()
      ? targetCommand
          .getAllTargets([targetCommand.TYPES.FRAME])
          .find(target => target.url == IFRAME_URL)
      : targetCommand.targetFront;
  for (let i = 0; i < expectedRuntimeConsoleCalls.length; i++) {
    const resource = availableResources[i];
    const { message, targetFront } = resource;
    is(
      targetFront,
      expectedTargetFront,
      "The targetFront property is the expected one"
    );
    const expected = expectedRuntimeConsoleCalls[i];
    checkConsoleAPICall(message, expected);
    is(
      resource.isAlreadyExistingResource,
      false,
      "isAlreadyExistingResource is false since we're ignoring existing resources"
    );
  }

  targetCommand.destroy();
  await client.close();

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // registrationPromise is set by the test page.
    const registration = await content.wrappedJSObject.registrationPromise;
    registration.unregister();
  });
}

async function logExistingMessages(browser, executeInIframe) {
  let browsingContext = browser.browsingContext;
  if (executeInIframe) {
    browsingContext = await SpecialPowers.spawn(
      browser,
      [],
      function frameScript() {
        return content.document.querySelector("iframe").browsingContext;
      }
    );
  }
  return evalInBrowsingContext(browsingContext, function pageScript() {
    console.log("foobarBaz-log", undefined);
    console.info("foobarBaz-info", null);
    console.warn("foobarBaz-warn", document.body);
  });
}

/**
 * Helper function similar to spawn, but instead of executing the script
 * as a Frame Script, with privileges and including test harness in stacktraces,
 * execute the script as a regular page script, without privileges and without any
 * preceding stack.
 *
 * @param {BrowsingContext} The browsing context into which the script should be evaluated
 * @param {Function|String} The JS to execute in the browsing context
 *
 * @return {Promise} Which resolves once the JS is done executing in the page
 */
function evalInBrowsingContext(browsingContext, script) {
  return SpecialPowers.spawn(browsingContext, [String(script)], expr => {
    const document = content.document;
    const scriptEl = document.createElement("script");
    document.body.appendChild(scriptEl);
    // Force the immediate execution of the stringified JS function passed in `expr`
    scriptEl.textContent = "new " + expr;
    scriptEl.remove();
  });
}

// For both existing and runtime messages, we execute console API
// from a page script evaluated via evalInBrowsingContext.
// Records here the function used to execute the script in the page.
const EXPECTED_FUNCTION_NAME = "pageScript";

const NUMBER_REGEX = /^\d+$/;

function getExpectedExistingConsoleCalls(documentFilename) {
  return [
    {
      level: "log",
      filename: documentFilename,
      functionName: EXPECTED_FUNCTION_NAME,
      timeStamp: NUMBER_REGEX,
      arguments: ["foobarBaz-log", { type: "undefined" }],
    },
    {
      level: "info",
      filename: documentFilename,
      functionName: EXPECTED_FUNCTION_NAME,
      timeStamp: NUMBER_REGEX,
      arguments: ["foobarBaz-info", { type: "null" }],
    },
    {
      level: "warn",
      filename: documentFilename,
      functionName: EXPECTED_FUNCTION_NAME,
      timeStamp: NUMBER_REGEX,
      arguments: ["foobarBaz-warn", { type: "object", actor: /[a-z]/ }],
    },
  ];
}

const longString = new Array(DevToolsServer.LONG_STRING_LENGTH + 2).join("a");
function getExpectedRuntimeConsoleCalls(documentFilename) {
  const defaultStackFrames = [
    // This is the usage of "new " + expr from `evalInBrowsingContext`
    {
      filename: documentFilename,
      lineNumber: 1,
      columnNumber: NUMBER_REGEX,
    },
  ];

  return [
    {
      level: "log",
      filename: documentFilename,
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
      filename: documentFilename,
      functionName: EXPECTED_FUNCTION_NAME,
      timeStamp: NUMBER_REGEX,
      arguments: ["foobarBaz-info", { type: "null" }],
    },
    {
      level: "warn",
      filename: documentFilename,
      functionName: EXPECTED_FUNCTION_NAME,
      timeStamp: NUMBER_REGEX,
      arguments: ["foobarBaz-warn", { type: "object", actor: /[a-z]/ }],
    },
    {
      level: "debug",
      filename: documentFilename,
      functionName: EXPECTED_FUNCTION_NAME,
      timeStamp: NUMBER_REGEX,
      arguments: [{ type: "null" }],
    },
    {
      level: "trace",
      filename: documentFilename,
      functionName: EXPECTED_FUNCTION_NAME,
      timeStamp: NUMBER_REGEX,
      stacktrace: [
        {
          filename: documentFilename,
          functionName: EXPECTED_FUNCTION_NAME,
        },
        ...defaultStackFrames,
      ],
    },
    {
      level: "dir",
      filename: documentFilename,
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
      filename: documentFilename,
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
      level: "error",
      filename: documentFilename,
      functionName: "fromAsmJS",
      timeStamp: NUMBER_REGEX,
      arguments: ["foobarBaz-asmjs-error", { type: "undefined" }],

      stacktrace: [
        {
          filename: documentFilename,
          functionName: "fromAsmJS",
        },
        {
          filename: documentFilename,
          functionName: "inAsmJS2",
        },
        {
          filename: documentFilename,
          functionName: "inAsmJS1",
        },
        {
          filename: documentFilename,
          functionName: EXPECTED_FUNCTION_NAME,
        },
        ...defaultStackFrames,
      ],
    },
    {
      level: "log",
      filename: gTestPath,
      functionName: "frameScript",
      timeStamp: NUMBER_REGEX,
      arguments: [
        {
          type: "object",
          actor: /[a-z]/,
          class: "Restricted",
        },
      ],
    },
  ];
}

async function logRuntimeMessages(browser, executeInIframe) {
  let browsingContext = browser.browsingContext;
  if (executeInIframe) {
    browsingContext = await SpecialPowers.spawn(
      browser,
      [],
      function frameScript() {
        return content.document.querySelector("iframe").browsingContext;
      }
    );
  }
  // First inject LONG_STRING_LENGTH in global scope it order to easily use it after
  await evalInBrowsingContext(
    browsingContext,
    `function () {window.LONG_STRING_LENGTH = ${DevToolsServer.LONG_STRING_LENGTH};}`
  );
  await evalInBrowsingContext(browsingContext, function pageScript() {
    const _longString = new Array(window.LONG_STRING_LENGTH + 2).join("a");

    console.log("foobarBaz-log", undefined);

    console.log("Float from not a number: %f", "foo");
    console.log("Float from string: %f", "1.2");
    console.log("Float from number: %f", 1.3);

    console.info("foobarBaz-info", null);
    console.warn("foobarBaz-warn", document.documentElement);
    console.debug(null);
    console.trace();
    console.dir(document, location);
    console.log("foo", _longString);

    function fromAsmJS() {
      console.error("foobarBaz-asmjs-error", undefined);
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
  await SpecialPowers.spawn(browsingContext, [], function frameScript() {
    const sandbox = new Cu.Sandbox(null, { invisibleToDebugger: true });
    const sandboxObj = sandbox.eval("new Object");
    content.console.log(sandboxObj);
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
