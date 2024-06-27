"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.init(this);

add_task(async function() {
  let scriptUrl = Services.io.newFileURI(do_get_file("file_simple_script.js")).spec;


  let script1 = await ChromeUtils.compileScript(scriptUrl, {hasReturnValue: true});
  let script2 = await ChromeUtils.compileScript(scriptUrl, {hasReturnValue: false});

  equal(script1.url, scriptUrl, "Script URL is correct")
  equal(script2.url, scriptUrl, "Script URL is correct")

  equal(script1.hasReturnValue, true, "Script hasReturnValue property is correct")
  equal(script2.hasReturnValue, false, "Script hasReturnValue property is correct")


  // Test return-value version.

  let sandbox1 = Cu.Sandbox("http://example.com");
  let sandbox2 = Cu.Sandbox("http://example.org");

  let obj = script1.executeInGlobal(sandbox1);
  equal(Cu.getObjectPrincipal(obj).origin, "http://example.com", "Return value origin is correct");
  equal(obj.foo, "\u00ae", "Return value has the correct charset");

  obj = script1.executeInGlobal(sandbox2);
  equal(Cu.getObjectPrincipal(obj).origin, "http://example.org", "Return value origin is correct");
  equal(obj.foo, "\u00ae", "Return value has the correct charset");


  // Test no-return-value version.

  sandbox1.bar = null;
  equal(sandbox1.bar, null);

  obj = script2.executeInGlobal(sandbox1);
  equal(obj, undefined, "No-return script has no return value");

  equal(Cu.getObjectPrincipal(sandbox1.bar).origin, "http://example.com", "Object value origin is correct");
  equal(sandbox1.bar.foo, "\u00ae", "Object value has the correct charset");


  sandbox2.bar = null;
  equal(sandbox2.bar, null);

  obj = script2.executeInGlobal(sandbox2);
  equal(obj, undefined, "No-return script has no return value");

  equal(Cu.getObjectPrincipal(sandbox2.bar).origin, "http://example.org", "Object value origin is correct");
  equal(sandbox2.bar.foo, "\u00ae", "Object value has the correct charset");
});

add_task(async function test_syntaxError() {
  // Generate an artificially large script to force off-main-thread
  // compilation.
  let scriptUrl = `data:,${";".repeat(1024 * 1024)}(`;

  await Assert.rejects(
    ChromeUtils.compileScript(scriptUrl),
    SyntaxError);

  // Generate a small script to force main thread compilation.
  scriptUrl = `data:,;(`;

  await Assert.rejects(
    ChromeUtils.compileScript(scriptUrl),
    SyntaxError);
});

add_task(async function test_Error_filename() {
  // This function will be serialized as a data:-URL and called.
  function getMyError() {
    let err = new Error();
    return {
      fileName: err.fileName,
      stackFirstLine: err.stack.split("\n")[0],
    };
  }
  const scriptUrl = `data:,(${encodeURIComponent(getMyError)})()`;
  const dummyFilename = "dummy filename";
  let script1 = await ChromeUtils.compileScript(scriptUrl, { hasReturnValue: true });
  let script2 = await ChromeUtils.compileScript(scriptUrl, { hasReturnValue: true, filename: dummyFilename });

  equal(script1.url, scriptUrl, "Script URL is correct");
  equal(script2.url, "dummy filename", "Script URL overridden");

  let sandbox = Cu.Sandbox("http://example.com");
  let err1 = script1.executeInGlobal(sandbox);
  equal(err1.fileName, scriptUrl, "fileName is original script URL");
  equal(err1.stackFirstLine, `getMyError@${scriptUrl}:2:15`, "Stack has original URL");

  let err2 = script2.executeInGlobal(sandbox);
  equal(err2.fileName, dummyFilename, "fileName is overridden filename");
  equal(err2.stackFirstLine, `getMyError@${dummyFilename}:2:15`, "Stack has overridden URL");
});

add_task(async function test_invalid_url() {
  // In this test we want a URL that doesn't resolve to a valid file.
  // Moreover, the name is chosen such that it does not trigger the
  // CheckForBrokenChromeURL check.
  await Assert.rejects(
    ChromeUtils.compileScript("resource:///invalid.ftl"),
    /^Unable to load script: resource:\/\/\/invalid\.ftl$/
  );

  await Assert.rejects(
    ChromeUtils.compileScript("resource:///invalid.ftl", { filename: "bye bye" }),
    /^Unable to load script: bye bye$/
  );
});

/**
 * Assert that executeInGlobal throws a special exception when the content script throws.
 * And the content script exception is notified to the console.
 */
add_task(async function test_exceptions_in_webconsole() {
  const scriptUrl = `data:,throw new Error("foo")`;
  const script = await ChromeUtils.compileScript(scriptUrl);
  const sandbox = Cu.Sandbox("http://example.com");

  Assert.throws(() => script.executeInGlobal(sandbox),
    /Error: foo/,
    "Without reportException set to true, executeInGlobal throws an exception");

  info("With reportException, executeInGlobal doesn't throw, but notifies the console");
  const { messages } = await AddonTestUtils.promiseConsoleOutput(() => {
    script.executeInGlobal(sandbox, { reportExceptions: true });
  });

  info("Wait for the console message related to the content script exception");
  equal(messages.length, 1, "Got one console message");
  messages[0].QueryInterface(Ci.nsIScriptError);
  equal(messages[0].errorMessage, "Error: foo", "We are notified about the plain content script exception via the console");
  ok(messages[0].stack, "The message has a stack");
});
