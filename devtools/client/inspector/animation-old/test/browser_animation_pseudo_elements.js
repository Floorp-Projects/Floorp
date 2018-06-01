/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that animated pseudo-elements do show in the timeline.

add_task(async function() {
  await addTab(URL_ROOT + "doc_pseudo_elements.html");
  const {inspector, panel} = await openAnimationInspector();

  info("With <body> selected by default check the content of the timeline");
  is(getAnimationTimeBlocks(panel).length, 3, "There are 3 animations in the timeline");

  const targetNodes = getAnimationTargetNodes(panel);
  const getTargetNodeText = index => {
    const el = targetNodes[index].previewer.previewEl;
    return [...el.childNodes]
           .map(n => n.style.display === "none" ? "" : n.textContent)
           .join("");
  };

  is(getTargetNodeText(0), "body", "The first animated node is <body>");
  is(getTargetNodeText(1), "::before", "The second animated node is ::before");
  is(getTargetNodeText(2), "::after", "The third animated node is ::after");

  info("Getting the before and after nodeFronts");
  const bodyContainer = await getContainerForSelector("body", inspector);
  const getBodyChildNodeFront = index => {
    return bodyContainer.elt.children[1].childNodes[index].container.node;
  };
  const beforeNode = getBodyChildNodeFront(0);
  const afterNode = getBodyChildNodeFront(1);

  info("Select the ::before pseudo-element in the inspector");
  await selectNodeAndWaitForAnimations(beforeNode, inspector);
  is(getAnimationTimeBlocks(panel).length, 1, "There is 1 animation in the timeline");
  is(getAnimationTargetNodes(panel)[0].previewer.nodeFront,
     inspector.selection.nodeFront,
     "The right node front is displayed in the timeline");

  info("Select the ::after pseudo-element in the inspector");
  await selectNodeAndWaitForAnimations(afterNode, inspector);
  is(getAnimationTimeBlocks(panel).length, 1, "There is 1 animation in the timeline");
  is(getAnimationTargetNodes(panel)[0].previewer.nodeFront,
     inspector.selection.nodeFront,
     "The right node front is displayed in the timeline");
});
