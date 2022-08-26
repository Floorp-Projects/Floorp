/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check if an error with a longString stack is displayed as expected.

"use strict";

const MESSAGE = "Error with longString stack";
const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html><script>
  const x = new Error("longString stack");
  x.stack = "s@http://exampl.com:1:1\\n".repeat(1000);
  console.log("${MESSAGE}", x);
</script>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Wait for the error to be logged");
  const msgNode = await waitFor(() => findConsoleAPIMessage(hud, MESSAGE));
  ok(msgNode, `Error logged`);

  const errorNode = msgNode.querySelector(".objectBox-stackTrace");
  ok(errorNode, "The error object is logged as expected");
  ok(errorNode.textContent.includes("longString stack"));

  info("Wait until the stacktrace gets rendered");
  const stackTraceElement = await waitFor(() =>
    errorNode.querySelector(".frames")
  );

  ok(stackTraceElement, "There's a stacktrace element");
  ok(
    !!stackTraceElement.querySelectorAll(".frame .title").length,
    "Frames functions are displayed"
  );
  ok(
    !!stackTraceElement.querySelectorAll(".frame .location").length,
    "Frames location are displayed"
  );
});
