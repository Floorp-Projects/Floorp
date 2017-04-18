/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that animated pseudo-elements do show in the timeline.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_pseudo_elements.html");
  let {inspector, panel} = yield openAnimationInspector();
  let timeline = panel.animationsTimelineComponent;

  info("With <body> selected by default check the content of the timeline");
  is(timeline.timeBlocks.length, 3, "There are 3 animations in the timeline");

  let getTargetNodeText = index => {
    let el = timeline.targetNodes[index].previewer.previewEl;
    return [...el.childNodes]
           .map(n => n.style.display === "none" ? "" : n.textContent)
           .join("");
  };

  is(getTargetNodeText(0), "body", "The first animated node is <body>");
  is(getTargetNodeText(1), "::before", "The second animated node is ::before");
  is(getTargetNodeText(2), "::after", "The third animated node is ::after");

  info("Getting the before and after nodeFronts");
  let bodyContainer = yield getContainerForSelector("body", inspector);
  let getBodyChildNodeFront = index => {
    return bodyContainer.elt.children[1].childNodes[index].container.node;
  };
  let beforeNode = getBodyChildNodeFront(0);
  let afterNode = getBodyChildNodeFront(1);

  info("Select the ::before pseudo-element in the inspector");
  yield selectNodeAndWaitForAnimations(beforeNode, inspector);
  is(timeline.timeBlocks.length, 1, "There is 1 animation in the timeline");
  is(timeline.targetNodes[0].previewer.nodeFront,
     inspector.selection.nodeFront,
     "The right node front is displayed in the timeline");

  info("Select the ::after pseudo-element in the inspector");
  yield selectNodeAndWaitForAnimations(afterNode, inspector);
  is(timeline.timeBlocks.length, 1, "There is 1 animation in the timeline");
  is(timeline.targetNodes[0].previewer.nodeFront,
     inspector.selection.nodeFront,
     "The right node front is displayed in the timeline");
});
