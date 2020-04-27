/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around ERROR_MESSAGES
// Reproduces assertions from devtools/shared/webconsole/test/chrome/test_page_errors.html

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

// Create a simple server so we have a nice sourceName in the resources packets.
const httpServer = createTestHTTPServer();
httpServer.registerPathHandler(`/test_page_errors.html`, (req, res) => {
  res.setStatusLine(req.httpVersion, 200, "OK");
  res.write(`<meta charset=utf8>Test Error Messages`);
});

const TEST_URI = `http://localhost:${httpServer.identity.primaryPort}/test_page_errors.html`;

add_task(async function() {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  // Open a test tab
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab(TEST_URI);

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  const receivedMessages = [];
  // The expected messages are the errors, twice (once for cached messages, once for live messages)
  const expectedMessages = Array.from(expectedPageErrors.values()).concat(
    Array.from(expectedPageErrors.values())
  );

  info(
    "Log some errors *before* calling ResourceWatcher.watch in order to assert the behavior of already existing messages."
  );
  await triggerErrors(tab);

  let done;
  const onAllErrorReceived = new Promise(resolve => (done = resolve));

  await resourceWatcher.watch(
    [ResourceWatcher.TYPES.ERROR_MESSAGES],
    ({ resourceType, targetFront, resource }) => {
      const { pageError } = resource;

      if (!pageError.sourceName.includes("test_page_errors")) {
        info(`Ignore error from unknown source: "${pageError.sourceName}"`);
        return;
      }

      const index = receivedMessages.length;
      receivedMessages.push(pageError);

      info(`checking received page error #${index}: ${pageError.errorMessage}`);
      ok(pageError, "The resource has a pageError attribute");
      checkObject(pageError, expectedMessages[index]);

      if (receivedMessages.length == expectedMessages.length) {
        done();
      }
    }
  );

  info(
    "Now log errors *after* the call to ResourceWatcher.watch and after having received all existing messages"
  );
  await BrowserTestUtils.waitForCondition(
    () => receivedMessages.length === expectedPageErrors.size
  );
  await triggerErrors(tab);

  info("Waiting for all expected errors to be received");
  await onAllErrorReceived;
  ok(true, "All the expected errors were received");

  Services.console.reset();
  targetList.stopListening();
  await client.close();
});

/**
 * Triggers all the errors in the content page.
 */
async function triggerErrors(tab) {
  for (const [expression, expected] of expectedPageErrors.entries()) {
    if (
      !expected[noUncaughtException] &&
      !Services.appinfo.browserTabsRemoteAutostart
    ) {
      expectUncaughtException();
    }

    await ContentTask.spawn(tab.linkedBrowser, expression, function frameScript(
      expr
    ) {
      const document = content.document;
      const container = document.createElement("script");
      document.body.appendChild(container);
      container.textContent = expr;
      container.remove();
    });
    // Wait a bit between each messages, as uncaught promises errors are not emitted
    // right away.

    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(res => setTimeout(res, 10));
  }
}

const noUncaughtException = Symbol();

const expectedPageErrors = new Map([
  [
    "document.doTheImpossible();",
    {
      errorMessage: /doTheImpossible/,
      errorMessageName: undefined,
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: true,
      warning: false,
    },
  ],
  [
    "(42).toString(0);",
    {
      errorMessage: /radix/,
      errorMessageName: "JSMSG_BAD_RADIX",
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: true,
      warning: false,
    },
  ],
  [
    "'use strict'; (Object.freeze({name: 'Elsa', score: 157})).score = 0;",
    {
      errorMessage: /read.only/,
      errorMessageName: "JSMSG_READ_ONLY",
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: true,
      warning: false,
    },
  ],
  [
    "([]).length = -1",
    {
      errorMessage: /array length/,
      errorMessageName: "JSMSG_BAD_ARRAY_LENGTH",
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: true,
      warning: false,
    },
  ],
  [
    "'abc'.repeat(-1);",
    {
      errorMessage: /repeat count.*non-negative/,
      errorMessageName: "JSMSG_NEGATIVE_REPETITION_COUNT",
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: true,
      warning: false,
    },
  ],
  [
    "'a'.repeat(2e28);",
    {
      errorMessage: /repeat count.*less than infinity/,
      errorMessageName: "JSMSG_RESULTING_STRING_TOO_LARGE",
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: true,
      warning: false,
    },
  ],
  [
    "77.1234.toExponential(-1);",
    {
      errorMessage: /out of range/,
      errorMessageName: "JSMSG_PRECISION_RANGE",
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: true,
      warning: false,
    },
  ],
  [
    "function a() { return; 1 + 1; }",
    {
      errorMessage: /unreachable code/,
      errorMessageName: "JSMSG_STMT_AFTER_RETURN",
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: false,
      warning: true,
    },
  ],
  [
    "{let a, a;}",
    {
      errorMessage: /redeclaration of/,
      errorMessageName: "JSMSG_REDECLARED_VAR",
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: true,
      warning: false,
      notes: [
        {
          messageBody: /Previously declared at line/,
          frame: {
            source: /test_page_errors/,
          },
        },
      ],
    },
  ],
  [
    `var error = new TypeError("abc");
      error.name = "MyError";
      error.message = "here";
      throw error`,
    {
      errorMessage: /MyError: here/,
      errorMessageName: "",
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: true,
      warning: false,
    },
  ],
  [
    "DOMTokenList.prototype.contains.call([])",
    {
      errorMessage: /does not implement interface/,
      errorMessageName: "MSG_METHOD_THIS_DOES_NOT_IMPLEMENT_INTERFACE",
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: true,
      warning: false,
    },
  ],
  [
    `var error2 = new TypeError("abc");
      error2.name = "MyPromiseError";
      error2.message = "here2";
      Promise.reject(error2)`,
    {
      errorMessage: /MyPromiseError: here2/,
      errorMessageName: "",
      sourceName: /test_page_errors/,
      category: "content javascript",
      timeStamp: /^\d+$/,
      error: true,
      warning: false,
      [noUncaughtException]: true,
    },
  ],
]);
