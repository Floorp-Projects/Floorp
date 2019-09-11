"use strict";

// Tests that unhandled promise rejections generate the appropriate
// console messages.

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);

PromiseTestUtils.expectUncaughtRejection(/could not be cloned/);
PromiseTestUtils.expectUncaughtRejection(/Bleah/);

const filename = "resource://foo/Bar.jsm";

async function getSandboxMessages(sandbox, code) {
  let { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    Cu.evalInSandbox(code, sandbox, null, filename, 1);

    // We need two trips through the event loop for this error to be reported.
    await new Promise(executeSoon);
    await new Promise(executeSoon);
  });

  // xpcshell tests on OS-X sometimes include an extra warning, which we
  // unfortunately need to ignore:
  return messages.filter(
    msg =>
      !msg.message.includes(
        "No chrome package registered for chrome://branding/locale/brand.properties"
      )
  );
}

add_task(async function test_unhandled_dom_exception() {
  let sandbox = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal());
  sandbox.StructuredCloneHolder = StructuredCloneHolder;

  let messages = await getSandboxMessages(
    sandbox,
    `new Promise(() => {
      new StructuredCloneHolder(() => {});
    });`
  );

  equal(messages.length, 1, "Got one console message");

  let [msg] = messages;
  ok(msg instanceof Ci.nsIScriptError, "Message is a script error");
  equal(msg.sourceName, filename, "Got expected filename");
  equal(msg.lineNumber, 2, "Got expected line number");
  equal(
    msg.errorMessage,
    "DataCloneError: The object could not be cloned.",
    "Got expected error message"
  );
});

add_task(async function test_unhandled_dom_exception_wrapped() {
  let sandbox = Cu.Sandbox(
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://example.com/"
    )
  );
  Cu.exportFunction(
    function frick() {
      throw new Components.Exception(
        "Bleah.",
        Cr.NS_ERROR_FAILURE,
        Components.stack.caller
      );
    },
    sandbox,
    { defineAs: "frick" }
  );

  let messages = await getSandboxMessages(
    sandbox,
    `new Promise(() => {
      frick();
    });`
  );

  equal(messages.length, 1, "Got one console message");

  let [msg] = messages;
  ok(msg instanceof Ci.nsIScriptError, "Message is a script error");
  equal(msg.sourceName, filename, "Got expected filename");
  equal(msg.lineNumber, 2, "Got expected line number");
  equal(
    msg.errorMessage,
    "NS_ERROR_FAILURE: Bleah.",
    "Got expected error message"
  );
});
