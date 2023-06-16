/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check console.error calls with expandable object.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><h1>test console.error with objects</h1>";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const onMessagesLogged = waitForMessageByType(
    hud,
    "myError",
    ".console-api.error"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.console.error("myError", { a: "a", b: "b" });
  });
  const { node } = await onMessagesLogged;

  const objectInspectors = [...node.querySelectorAll(".tree")];
  is(
    objectInspectors.length,
    1,
    "There is the expected number of object inspectors"
  );
  const [oi] = objectInspectors;
  oi.querySelector(".node .arrow").click();
  await waitFor(() => oi.querySelectorAll(".node").length > 1);
  ok(true, "The object can be expanded");
});
