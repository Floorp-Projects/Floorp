"use strict";

// Tests that unhandled promise rejections generate the appropriate
// console messages.

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");

PromiseTestUtils.expectUncaughtRejection(/could not be cloned/);

add_task(async function test_unhandled_dom_exception() {
  let sandbox = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal());
  sandbox.StructuredCloneHolder = StructuredCloneHolder;

  let filename = "resource://foo/Bar.jsm";

  let {messages} = await AddonTestUtils.promiseConsoleOutput(async () => {
    let code = `new Promise(() => {
      new StructuredCloneHolder(() => {});
    });`;

    Cu.evalInSandbox(code, sandbox, null, filename, 1);

    // We need two trips through the event loop for this error to be reported.
    await new Promise(executeSoon);
    await new Promise(executeSoon);
  });

  // xpcshell tests on OS-X sometimes include an extra warning, which we
  // unfortunately need to ignore:
  messages = messages.filter(msg => !msg.message.includes(
    "No chrome package registered for chrome://branding/locale/brand.properties"));

  equal(messages.length, 1, "Got one console message");

  let [msg] = messages;
  ok(msg instanceof Ci.nsIScriptError, "Message is a script error");
  equal(msg.sourceName, filename, "Got expected filename");
  equal(msg.lineNumber, 2, "Got expected line number");
  equal(msg.errorMessage, "DataCloneError: The object could not be cloned.",
        "Got expected error message");
});
