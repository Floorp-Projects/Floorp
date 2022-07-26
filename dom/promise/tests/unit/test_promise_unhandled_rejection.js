"use strict";

// Tests that unhandled promise rejections generate the appropriate
// console messages.

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);

PromiseTestUtils.expectUncaughtRejection(/could not be cloned/);
PromiseTestUtils.expectUncaughtRejection(/An exception was thrown/);
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
    "DataCloneError: Function object could not be cloned.",
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

  equal(messages.length, 2, "Got two console messages");

  let [msg1, msg2] = messages;
  ok(msg1 instanceof Ci.nsIScriptError, "Message is a script error");
  equal(msg1.sourceName, filename, "Got expected filename");
  equal(msg1.lineNumber, 2, "Got expected line number");
  equal(
    msg1.errorMessage,
    "NS_ERROR_FAILURE: Bleah.",
    "Got expected error message"
  );

  ok(msg2 instanceof Ci.nsIScriptError, "Message is a script error");
  equal(msg2.sourceName, filename, "Got expected filename");
  equal(msg2.lineNumber, 2, "Got expected line number");
  equal(
    msg2.errorMessage,
    "InvalidStateError: An exception was thrown",
    "Got expected error message"
  );
});

add_task(async function test_unhandled_dom_exception_from_sandbox() {
  let sandbox = Cu.Sandbox(
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://example.com/"
    ),
    { wantGlobalProperties: ["DOMException"] }
  );
  let ctor = Cu.evalInSandbox("DOMException", sandbox);
  Cu.exportFunction(
    function frick() {
      throw new ctor("Bleah.");
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

  equal(messages.length, 1, "Got one console messages");

  let [msg] = messages;
  ok(msg instanceof Ci.nsIScriptError, "Message is a script error");
  equal(msg.sourceName, filename, "Got expected filename");
  equal(msg.lineNumber, 2, "Got expected line number");
  equal(msg.errorMessage, "Error: Bleah.", "Got expected error message");
});
