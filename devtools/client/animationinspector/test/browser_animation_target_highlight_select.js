/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that the DOM element targets displayed in animation player widgets can
// be used to highlight elements in the DOM and select them in the inspector.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");

  let {toolbox, inspector, panel} = await openAnimationInspector();

  info("Select the simple animated node");
  await selectNodeAndWaitForAnimations(".animated", inspector);

  let targets = getAnimationTargetNodes(panel);
  // Arbitrary select the first one
  let targetNodeComponent = targets[0];

  info("Retrieve the part of the widget that highlights the node on hover");
  let highlightingEl = targetNodeComponent.previewer.previewEl;

  info("Listen to node-highlight event and mouse over the widget");
  let onHighlight = toolbox.once("node-highlight");
  EventUtils.synthesizeMouse(highlightingEl, 10, 5, {type: "mouseover"},
                             highlightingEl.ownerDocument.defaultView);
  let nodeFront = await onHighlight;

  // Do not forget to mouseout, otherwise we get random mouseover event
  // when selecting another node, which triggers some requests in animation
  // inspector.
  EventUtils.synthesizeMouse(highlightingEl, 10, 5, {type: "mouseout"},
                             highlightingEl.ownerDocument.defaultView);

  ok(true, "The node-highlight event was fired");
  is(targetNodeComponent.previewer.nodeFront, nodeFront,
    "The highlighted node is the one stored on the animation widget");
  is(nodeFront.tagName, "DIV",
    "The highlighted node has the correct tagName");
  is(nodeFront.attributes[0].name, "class",
    "The highlighted node has the correct attributes");
  is(nodeFront.attributes[0].value, "ball animated",
    "The highlighted node has the correct class");

  info("Select the body node in order to have the list of all animations");
  await selectNodeAndWaitForAnimations("body", inspector);

  targets = getAnimationTargetNodes(panel);
  targetNodeComponent = targets[0];

  info("Click on the first animated node component and wait for the " +
       "selection to change");
  let onSelection = inspector.selection.once("new-node-front");
  let onRendered = waitForAnimationTimelineRendering(panel);
  let nodeEl = targetNodeComponent.previewer.previewEl;
  EventUtils.sendMouseEvent({type: "click"}, nodeEl,
                            nodeEl.ownerDocument.defaultView);
  await onSelection;

  is(inspector.selection.nodeFront, targetNodeComponent.previewer.nodeFront,
    "The selected node is the one stored on the animation widget");

  await onRendered;
});
