/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the Javascript Tracing feature via the Web Console :trace command.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>
<body>
  <div>
    <h1>Testing trace command</h1>
    <script>
    function main() {}
    function someNoise() {}
    </script>
  </div>
  <div><p></p></div>
</body>`;

add_task(async function testBasicRecord() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  const hud = await openNewTabAndConsole(TEST_URI);
  ok(hud, "web console opened");

  info("Test unsupported param error message");
  let msg = await evaluateExpressionInConsole(
    hud,
    ":trace --unsupported-param",
    "console-api"
  );
  is(
    msg.textContent.trim(),
    ":trace command doesn't support 'unsupported-param' argument."
  );

  info("Test the help argument");
  msg = await evaluateExpressionInConsole(hud, ":trace --help", "console-api");
  ok(msg.textContent.includes("Toggles the JavaScript tracer"));

  info("Test toggling the tracer ON");
  // Pass `console-api` specific classname as the command results don't log anything.
  // Instead a JSTRACER_STATE resource logs a console-api message.
  msg = await evaluateExpressionInConsole(
    hud,
    ":trace --logMethod console --prefix foo --values --on-next-interaction",
    "console-api"
  );
  is(msg.textContent.trim(), "Started tracing to Web Console");

  info("Trigger some code before the user interaction");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.someNoise();
  });

  info("Simulate a user interaction by trigerring a key event on the page");
  await BrowserTestUtils.synthesizeKey("a", {}, gBrowser.selectedBrowser);

  info("Trigger some code to log some traces");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.main("arg", 2);
  });

  // Assert that we also see the custom prefix, as well as function arguments
  await waitFor(
    () => !!findTracerMessages(hud, `foo: interpreter⟶λ main("arg", 2)`).length
  );
  is(
    findTracerMessages(hud, `someNoise`).length,
    0,
    "The code running before the key press should not be traced"
  );

  // But now that the tracer is active, we will be able to log this call to someNoise
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.someNoise();
  });
  await waitFor(
    () => !!findTracerMessages(hud, `foo: interpreter⟶λ someNoise()`).length
  );

  info("Test toggling the tracer OFF");
  msg = await evaluateExpressionInConsole(hud, ":trace", "console-api");
  is(msg.textContent.trim(), "Stopped tracing");

  info("Clear past traces");
  hud.ui.clearOutput();
  await waitFor(
    () => !findTracerMessages(hud, `foo: interpreter⟶λ main("arg", 2)`).length
  );
  ok("Console was cleared");

  info("Trigger some code again");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.main();
  });

  // Let some time for traces to appear
  await wait(1000);

  ok(
    !findTracerMessages(hud, `foo: interpreter⟶λ main("arg", 2)`).length,
    "We really stopped recording traces, and no trace appear in the console"
  );
});

add_task(async function testLimitedRecord() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  const jsCode = `function foo1() {foo2()}; function foo2() {foo3()}; function foo3() {}; function bar() {}`;
  const hud = await openNewTabAndConsole(
    "data:text/html," + encodeURIComponent(`<script>${jsCode}</script>`)
  );
  ok(hud, "web console opened");

  info("Test toggling the tracer ON");
  // Pass `console-api` specific classname as the command results aren't logged as "result".
  // Instead the frontend log a message as a console API message.
  await evaluateExpressionInConsole(
    hud,
    ":trace --logMethod console --max-depth 1",
    "console-api"
  );

  info("Execute some code trigerring the tracer");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.foo1();
  });

  await waitFor(() => !!findTracerMessages(hud, `λ foo1`).length);
  ok(true, "foo1 trace was printed to console");

  is(
    findTracerMessages(hud, `λ foo2`).length,
    0,
    "We only see the first function thanks to the depth limit"
  );

  info("Test toggling the tracer OFF");
  await evaluateExpressionInConsole(hud, ":trace", "console-api");

  info("Clear past traces");
  hud.ui.clearOutput();
  await waitFor(() => !findTracerMessages(hud, `λ foo1`).length);
  ok("Console was cleared");

  info("Re-enable the tracing, but with a max record limit");
  await evaluateExpressionInConsole(
    hud,
    ":trace --logMethod console --max-records 1",
    "console-api"
  );

  info("Trigger some code again");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.foo1();
    content.wrappedJSObject.bar();
  });

  await waitFor(() => !!findTracerMessages(hud, `λ foo3`).length);
  await waitFor(() => !!findConsoleAPIMessage(hud, `Stopped tracing`));
  is(
    findTracerMessages(hud, `λ foo1`).length,
    1,
    "Found the whole depth for the first event loop 1/3"
  );
  is(
    findTracerMessages(hud, `λ foo2`).length,
    1,
    "Found the whole depth for the first event loop 2/3"
  );
  is(
    findTracerMessages(hud, `λ foo3`).length,
    1,
    "Found the whole depth for the first event loop 3/3"
  );
  is(
    findTracerMessages(hud, `λ bar`).length,
    0,
    "But the second event loop was ignored"
  );
  ok(
    !!findConsoleAPIMessage(
      hud,
      `Stopped tracing (reason: max-records)`,
      ".console-api"
    ),
    "And the tracer was automatically stopped"
  );

  info("Enable tracing one last time without any restriction");
  await evaluateExpressionInConsole(
    hud,
    ":trace --logMethod console",
    "console-api"
  );
  info("Trigger various code again");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.foo1();
    content.wrappedJSObject.bar();
  });
  await waitFor(() => !!findTracerMessages(hud, `λ foo1`).length);
  await waitFor(() => !!findTracerMessages(hud, `λ foo2`).length);
  await waitFor(() => !!findTracerMessages(hud, `λ foo3`).length);
  await waitFor(() => !!findTracerMessages(hud, `λ bar`).length);
  ok(true, "All traces were printed to console");
});
