/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check expanding/collapsing object inspector in the console when text is selected.
const TEST_URI = "data:text/html;charset=utf8,<h1>test Object Inspector</h1>";

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");
  const hud = toolbox.getCurrentPanel().hud;

  const label = "oi-test";
  const onLoggedMessage = waitForMessage(hud, label);
  await ContentTask.spawn(gBrowser.selectedBrowser, label, function(str) {
    content.wrappedJSObject.console.log(str, [1, 2, 3]);
  });
  const {node} = await onLoggedMessage;

  info(`Select the "Array" text`);
  selectNode(hud, node.querySelector(".objectTitle"));

  info("Click on the arrow to expand the object");
  node.querySelector(".arrow").click();
  await waitFor(() => node.querySelectorAll(".tree-node").length > 1);
  ok(true, "The array was expanded as expected");
});
