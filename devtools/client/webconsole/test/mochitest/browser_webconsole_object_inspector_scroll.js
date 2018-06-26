/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that expanding an objectInspector node doesn't alter the output scroll position.
const TEST_URI = "data:text/html;charset=utf8,test Object Inspector";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.console.log("oi-test", content.wrappedJSObject.Math);
  });

  const node = await waitFor(() => findMessage(hud, "oi-test"));
  const objectInspector = node.querySelector(".tree");

  let onOiMutation = waitForNodeMutation(objectInspector, {
    childList: true
  });

  info("Expanding the object inspector");
  objectInspector.querySelector(".arrow").click();
  await onOiMutation;

  const nodes = objectInspector.querySelectorAll(".node");
  const lastNode = nodes[nodes.length - 1];

  info("Scroll the last node of the ObjectInspector into view");
  lastNode.scrollIntoView();

  const outputContainer = hud.ui.outputNode.querySelector(".webconsole-output");
  ok(hasVerticalOverflow(outputContainer), "There is a vertical overflow");
  const scrollTop = outputContainer.scrollTop;

  onOiMutation = waitForNodeMutation(objectInspector, {
    childList: true
  });

  info("Expand the last node");
  const view = lastNode.ownerDocument.defaultView;
  EventUtils.synthesizeMouseAtCenter(lastNode, {}, view);

  await onOiMutation;

  is(scrollTop, outputContainer.scrollTop,
    "Scroll position did not changed when expanding a node");
});

function hasVerticalOverflow(container) {
  return container.scrollHeight > container.clientHeight;
}
