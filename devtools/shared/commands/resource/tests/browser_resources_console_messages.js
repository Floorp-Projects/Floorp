/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around CONSOLE_MESSAGE
//
// Reproduces assertions from: devtools/shared/webconsole/test/chrome/test_cached_messages.html
// And now more. Once we remove the console actor's startListeners in favor of watcher class
// We could remove that other old test.

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

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  info(
    "Log some messages *before* calling ResourceCommand.watchResources in order to " +
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
      is(
        resource.resourceType,
        resourceCommand.TYPES.CONSOLE_MESSAGE,
        "Received a message"
      );
      ok(resource.message, "message is wrapped into a message attribute");
      const isCachedMessage = !!expectedExistingCalls.length;
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

      if (!expectedRuntimeCalls.length) {
        runtimeDoneResolve();
      }
    }
  };

  await resourceCommand.watchResources(
    [resourceCommand.TYPES.CONSOLE_MESSAGE],
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
    "Now log messages *after* the call to ResourceCommand.watchResources and after having received all existing messages"
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
}

async function testTabConsoleMessagesResourcesWithIgnoreExistingResources(
  executeInIframe
) {
  info("Test ignoreExistingResources option for console messages");
  const tab = await addTab(FISSION_TEST_URL);

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  info(
    "Check whether onAvailable will not be called with existing console messages"
  );
  await logExistingMessages(tab.linkedBrowser, executeInIframe);

  const availableResources = [];
  await resourceCommand.watchResources(
    [resourceCommand.TYPES.CONSOLE_MESSAGE],
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
    executeInIframe && (isFissionEnabled() || isEveryFrameTargetEnabled())
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
// timeStamp are the result of a number in microsecond divided by 1000.
// so we can't expect a precise number of decimals, or even if there would
// be decimals at all.
const FRACTIONAL_NUMBER_REGEX = /^\d+(\.\d{1,3})?$/;

function getExpectedExistingConsoleCalls(documentFilename) {
  const defaultProperties = {
    filename: documentFilename,
    columnNumber: NUMBER_REGEX,
    lineNumber: NUMBER_REGEX,
    timeStamp: FRACTIONAL_NUMBER_REGEX,
    innerWindowID: NUMBER_REGEX,
    chromeContext: undefined,
    counter: undefined,
    prefix: undefined,
    private: undefined,
    stacktrace: undefined,
    styles: undefined,
    timer: undefined,
  };

  return [
    {
      ...defaultProperties,
      level: "log",
      arguments: ["foobarBaz-log", { type: "undefined" }],
    },
    {
      ...defaultProperties,
      level: "info",
      arguments: ["foobarBaz-info", { type: "null" }],
    },
    {
      ...defaultProperties,
      level: "warn",
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
      lineNumber: NUMBER_REGEX,
      columnNumber: NUMBER_REGEX,
    },
  ];

  const defaultProperties = {
    filename: documentFilename,
    columnNumber: NUMBER_REGEX,
    lineNumber: NUMBER_REGEX,
    timeStamp: FRACTIONAL_NUMBER_REGEX,
    innerWindowID: NUMBER_REGEX,
    chromeContext: undefined,
    counter: undefined,
    prefix: undefined,
    private: undefined,
    stacktrace: undefined,
    styles: undefined,
    timer: undefined,
  };

  return [
    {
      ...defaultProperties,
      level: "log",
      arguments: ["foobarBaz-log", { type: "undefined" }],
    },
    {
      ...defaultProperties,
      level: "log",
      arguments: ["Float from not a number: NaN"],
    },
    {
      ...defaultProperties,
      level: "log",
      arguments: ["Float from string: 1.200000"],
    },
    {
      ...defaultProperties,
      level: "log",
      arguments: ["Float from number: 1.300000"],
    },
    {
      ...defaultProperties,
      level: "log",
      arguments: ["message with ", "style"],
      styles: ["color: blue;", "background: red; font-size: 2em;"],
    },
    {
      ...defaultProperties,
      level: "info",
      arguments: ["foobarBaz-info", { type: "null" }],
    },
    {
      ...defaultProperties,
      level: "warn",
      arguments: ["foobarBaz-warn", { type: "object", actor: /[a-z]/ }],
    },
    {
      ...defaultProperties,
      level: "debug",
      arguments: [{ type: "null" }],
    },
    {
      ...defaultProperties,
      level: "trace",
      stacktrace: [
        {
          filename: documentFilename,
          functionName: EXPECTED_FUNCTION_NAME,
        },
        ...defaultStackFrames,
      ],
    },
    {
      ...defaultProperties,
      level: "dir",
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
      ...defaultProperties,
      level: "log",
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
      ...defaultProperties,
      level: "count",
      arguments: ["myCounter"],
      counter: {
        count: 1,
        label: "myCounter",
      },
    },
    {
      ...defaultProperties,
      level: "count",
      arguments: ["myCounter"],
      counter: {
        count: 2,
        label: "myCounter",
      },
    },
    {
      ...defaultProperties,
      level: "count",
      arguments: ["default"],
      counter: {
        count: 1,
        label: "default",
      },
    },
    {
      ...defaultProperties,
      level: "countReset",
      arguments: ["myCounter"],
      counter: {
        count: 0,
        label: "myCounter",
      },
    },
    {
      ...defaultProperties,
      level: "countReset",
      arguments: ["unknownCounter"],
      counter: {
        error: "counterDoesntExist",
        label: "unknownCounter",
      },
    },
    {
      ...defaultProperties,
      level: "time",
      arguments: ["myTimer"],
      timer: {
        name: "myTimer",
      },
    },
    {
      ...defaultProperties,
      level: "time",
      arguments: ["myTimer"],
      timer: {
        name: "myTimer",
        error: "timerAlreadyExists",
      },
    },
    {
      ...defaultProperties,
      level: "timeLog",
      arguments: ["myTimer"],
      timer: {
        name: "myTimer",
        duration: NUMBER_REGEX,
      },
    },
    {
      ...defaultProperties,
      level: "timeEnd",
      arguments: ["myTimer"],
      timer: {
        name: "myTimer",
        duration: NUMBER_REGEX,
      },
    },
    {
      ...defaultProperties,
      level: "time",
      arguments: ["default"],
      timer: {
        name: "default",
      },
    },
    {
      ...defaultProperties,
      level: "timeLog",
      arguments: ["default"],
      timer: {
        name: "default",
        duration: NUMBER_REGEX,
      },
    },
    {
      ...defaultProperties,
      level: "timeEnd",
      arguments: ["default"],
      timer: {
        name: "default",
        duration: NUMBER_REGEX,
      },
    },
    {
      ...defaultProperties,
      level: "timeLog",
      arguments: ["unknownTimer"],
      timer: {
        name: "unknownTimer",
        error: "timerDoesntExist",
      },
    },
    {
      ...defaultProperties,
      level: "timeEnd",
      arguments: ["unknownTimer"],
      timer: {
        name: "unknownTimer",
        error: "timerDoesntExist",
      },
    },
    {
      ...defaultProperties,
      level: "error",
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
      ...defaultProperties,
      level: "log",
      filename:
        "chrome://mochitests/content/browser/devtools/shared/commands/resource/tests/browser_resources_console_messages.js",
      arguments: [
        {
          type: "object",
          actor: /[a-z]/,
          class: "Restricted",
        },
      ],
      chromeContext: true,
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
    console.log(
      "%cmessage with %cstyle",
      "color: blue;",
      "background: red; font-size: 2em;"
    );

    console.info("foobarBaz-info", null);
    console.warn("foobarBaz-warn", document.documentElement);
    console.debug(null);
    console.trace();
    console.dir(document, location);
    console.log("foo", _longString);

    console.count("myCounter");
    console.count("myCounter");
    console.count();
    console.countReset("myCounter");
    // will cause warnings because unknownCounter doesn't exist
    console.countReset("unknownCounter");

    console.time("myTimer");
    // will cause warning because myTimer already exist
    console.time("myTimer");
    console.timeLog("myTimer");
    console.timeEnd("myTimer");
    console.time();
    console.timeLog();
    console.timeEnd();
    // // will cause warnings because unknownTimer doesn't exist
    console.timeLog("unknownTimer");
    console.timeEnd("unknownTimer");

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
    })(null, { fromAsmJS })();
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
