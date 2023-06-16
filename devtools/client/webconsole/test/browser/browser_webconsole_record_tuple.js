/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check logging records and tuples in the console.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>" +
  encodeURIComponent(
    `<script>console.log("oi-test", #{hello: "world"}, #[42])</script>
    <h1>Object Inspector on records and tuples</h1>`
  );

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const hasSupport = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      return typeof content.wrappedJSObject.Record == "function";
    }
  );

  if (!hasSupport) {
    ok(true, "Records and Tuples not supported yet");
    return;
  }

  const node = await waitFor(() => findConsoleAPIMessage(hud, "oi-test"));
  ok(node.textContent.includes("Record"), "Record is displayed as expected");
  ok(node.textContent.includes("Tuple"), "Tuple is displayed as expected");
});
