/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that top-level await with dynamic import works as expected.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-dynamic-import.html";

add_task(async function() {
  // Enable dynamic import
  await pushPref("javascript.options.dynamicImport", true);
  // Enable await mapping.
  await pushPref("devtools.debugger.features.map-await-expression", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  const executeAndWaitForResultMessage = (input, expectedOutput) =>
    executeAndWaitForMessage(hud, input, expectedOutput, ".result");

  info("Evaluate an expression with a dynamic import");
  let importAwaitExpression = `
    var {sum} = await import("./test-dynamic-import.js");
    sum(1, 2, 3);
  `;
  await executeAndWaitForResultMessage(importAwaitExpression, `1 + 2 + 3 = 6`);
  ok(true, "The `sum` module was imported and used successfully");

  info("Import the same module a second time");
  // This used to make the content page crash (See Bug 1523897).
  importAwaitExpression = `
    var {sum} = await import("./test-dynamic-import.js");
    sum(2, 3, 4);
  `;
  await executeAndWaitForResultMessage(importAwaitExpression, `2 + 3 + 4 = 9`);
  ok(true, "The `sum` module was imported and used successfully a second time");
});
