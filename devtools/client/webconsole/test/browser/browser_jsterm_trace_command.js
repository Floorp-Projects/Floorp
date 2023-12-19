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
    </script>
  </div>
  <div><p></p></div>
</body>`;

add_task(async function () {
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
  // Pass `console-api` specific classname as the command results aren't logged as "result".
  // Instead the frontend log a message as a console API message.
  msg = await evaluateExpressionInConsole(
    hud,
    ":trace --logMethod console --prefix foo --values",
    "console-api"
  );
  is(msg.textContent.trim(), "Started tracing to Web Console");

  info("Trigger some code to log some traces");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.main("arg", 2);
  });

  // Assert that we also see the custom prefix, as well as function arguments
  await waitFor(
    () => !!findTracerMessages(hud, `foo: interpreter⟶λ main("arg", 2)`).length
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
