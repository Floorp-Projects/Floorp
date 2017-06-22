"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

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
