/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check expanding/collapsing object inspector in the console when text is selected.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><h1>test Object Inspector</h1>";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const label = "oi-test";
  const onLoggedMessage = waitForMessageByType(hud, label, ".console-api");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [label], function (str) {
    content.wrappedJSObject.console.log(str, [1, 2, 3]);
  });
  const { node } = await onLoggedMessage;

  info(`Select the "Array" text`);
  selectNode(hud, node.querySelector(".objectTitle"));

  info("Click on the arrow to expand the object");
  node.querySelector(".arrow").click();
  await waitFor(() => node.querySelectorAll(".tree-node").length > 1);
  ok(true, "The array was expanded as expected");
});
