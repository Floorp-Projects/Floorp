/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the computed view refreshes when the current node has its style
// changed.

const TEST_URI = "<div id='testdiv' style='font-size:10px;'>Test div!</div>";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view, testActor} = await openComputedView();
  await selectNode("#testdiv", inspector);

  let fontSize = getComputedViewPropertyValue(view, "font-size");
  is(fontSize, "10px", "The computed view shows the right font-size");

  info("Changing the node's style and waiting for the update");
  const onUpdated = inspector.once("computed-view-refreshed");
  await testActor.setAttribute("#testdiv", "style",
                               "font-size: 15px; color: red;");
  await onUpdated;

  fontSize = getComputedViewPropertyValue(view, "font-size");
  is(fontSize, "15px", "The computed view shows the updated font-size");
  const color = getComputedViewPropertyValue(view, "color");
  is(color, "rgb(255, 0, 0)", "The computed view also shows the color now");
});
