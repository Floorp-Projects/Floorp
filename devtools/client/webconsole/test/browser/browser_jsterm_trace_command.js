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
    function main() { return 42; }
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
    ":trace --logMethod console --prefix foo --returns --values --on-next-interaction",
    "console-api"
  );
  is(
    msg.textContent.trim(),
    "Waiting for next user interaction before tracing (next mousedown or keydown event)"
  );

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

  info("Ensure a message notified about the tracer actual start");
  await waitFor(
    () => !!findConsoleAPIMessage(hud, `Started tracing to Web Console`)
  );

  // Assert that we also see the custom prefix, as well as function arguments
  await waitFor(
    () =>
      !!findTracerMessages(hud, `foo: ⟶ interpreter λ main("arg", 2)`).length
  );
  is(
    findTracerMessages(hud, `someNoise`).length,
    0,
    "The code running before the key press should not be traced"
  );
  await waitFor(
    () => !!findTracerMessages(hud, `foo: ⟵ λ main return 42`).length,

    "Got the function returns being logged, with the returned value"
  );

  // But now that the tracer is active, we will be able to log this call to someNoise
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.someNoise();
  });
  await waitFor(
    () => !!findTracerMessages(hud, `foo: ⟶ interpreter λ someNoise()`).length
  );

  info("Test toggling the tracer OFF");
  msg = await evaluateExpressionInConsole(hud, ":trace", "console-api");
  is(msg.textContent.trim(), "Stopped tracing");

  info("Clear past traces");
  hud.ui.clearOutput();
  await waitFor(
    () => !findTracerMessages(hud, `foo: ⟶ interpreter λ main("arg", 2)`).length
  );
  ok("Console was cleared");

  info("Trigger some code again");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.main();
  });

  // Let some time for traces to appear
  await wait(500);

  ok(
    !findTracerMessages(hud, `foo: ⟶ interpreter λ main("arg", 2)`).length,
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

add_task(async function testDOMMutations() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  const testScript = `/* this will be line 1 */
  function add() {
    const element = document.createElement("hr");
    document.body.appendChild(element);
  }
  function attributes() {
    document.querySelector("hr").setAttribute("hidden", "true");
  }
  function remove() {
    document.querySelector("hr").remove();
  }
  /* Fake a real file name for this inline script */
  //# sourceURL=fake.js
  `;
  const hud = await openNewTabAndConsole(
    `data:text/html,test-page<script>${encodeURIComponent(testScript)}</script>`
  );
  ok(hud, "web console opened");

  let msg = await evaluateExpressionInConsole(
    hud,
    ":trace --dom-mutations foo",
    "console-api"
  );
  is(
    msg.textContent.trim(),
    ":trace --dom-mutations only accept a list of strings whose values can be: add,attributes,remove"
  );

  msg = await evaluateExpressionInConsole(
    hud,
    ":trace --dom-mutations 42",
    "console-api"
  );
  is(
    msg.textContent.trim(),
    ":trace --dom-mutations accept only no arguments, or a list mutation type strings (add,attributes,remove)"
  );

  info("Test toggling the tracer ON");
  // Pass `console-api` specific classname as the command results aren't logged as "result".
  // Instead the frontend log a message as a console API message.
  await evaluateExpressionInConsole(
    hud,
    ":trace --logMethod console --dom-mutations",
    "console-api"
  );

  info("Trigger some code to add a DOM Element");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.add();
  });
  let traceNode = await waitFor(
    () => findTracerMessages(hud, `DOM Mutation | add <hr>`)[0],
    "Wait for the DOM Mutation trace for DOM element creation"
  );
  is(
    traceNode.querySelector(".message-location").textContent,
    "fake.js:4:19",
    "Add Mutation location is correct"
  );

  info("Trigger some code to modify attributes of a DOM Element");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.attributes();
  });
  traceNode = await waitFor(
    () =>
      findTracerMessages(
        hud,
        `DOM Mutation | attributes <hr hidden="true">`
      )[0],
    "Wait for the DOM Mutation trace for DOM attributes modification"
  );
  is(
    traceNode.querySelector(".message-location").textContent,
    "fake.js:7:34",
    "Attributes Mutation location is correct"
  );

  info("Trigger some code to remove a DOM Element");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.remove();
  });
  traceNode = await waitFor(
    () =>
      findTracerMessages(hud, `DOM Mutation | remove <hr hidden="true">`)[0],
    "Wait for the DOM Mutation trace for DOM Element removal"
  );
  is(
    traceNode.querySelector(".message-location").textContent,
    "fake.js:10:34",
    "Remove Mutation location is correct"
  );

  info("Stop tracing all mutations");
  await evaluateExpressionInConsole(hud, ":trace", "console-api");

  info("Clear past traces");
  hud.ui.clearOutput();
  await waitFor(
    () => !findTracerMessages(hud, `remove(<hr hidden="true">)`).length
  );
  ok("Console was cleared");

  info("Re-enable the tracing, but only with a subset of mutations");
  await evaluateExpressionInConsole(
    hud,
    ":trace --logMethod console --dom-mutations attributes,remove",
    "console-api"
  );

  info("Trigger all types of mutations");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const element = content.document.createElement("hr");
    content.document.body.appendChild(element);
    element.setAttribute("hidden", "true");
    element.remove();
  });
  await waitFor(
    () =>
      !!findTracerMessages(hud, `DOM Mutation | attributes <hr hidden="true">`)
        .length,
    "Wait for the DOM Mutation trace for DOM attributes modification"
  );

  await waitFor(
    () =>
      !!findTracerMessages(hud, `DOM Mutation | remove <hr hidden="true">`)
        .length,
    "Wait for the DOM Mutation trace for DOM Element removal"
  );
  is(
    findTracerMessages(hud, `add <hr`).length,
    0,
    "DOM Element creation isn't traced"
  );
});
