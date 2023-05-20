/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following selection feature related AnimationTarget component works:
// * select selected node by clicking on target node

add_task(async function () {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".multi", ".long"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Check initial status");
  is(
    panel.querySelectorAll(".animation-item").length,
    3,
    "The length of animations should be 3. Two .multi animations and one .long animation"
  );

  info("Check selecting an animated node by clicking on the target node");
  await clickOnTargetNode(animationInspector, panel, 0);
  assertNodeFront(
    animationInspector.inspector.selection.nodeFront,
    "DIV",
    "ball multi"
  );
  is(
    panel.querySelectorAll(".animation-item").length,
    2,
    "The length of animations should be 2"
  );

  info("Check if the both target nodes refer to the same node");
  await clickOnTargetNode(animationInspector, panel, 1);
  assertNodeFront(
    animationInspector.inspector.selection.nodeFront,
    "DIV",
    "ball multi"
  );
  is(
    panel.querySelectorAll(".animation-item").length,
    2,
    "The length of animations should be 2"
  );
});

function assertNodeFront(nodeFront, tagName, classValue) {
  is(
    nodeFront.tagName,
    tagName,
    "The highlighted node has the correct tagName"
  );
  is(
    nodeFront.attributes[0].name,
    "class",
    "The highlighted node has the correct attributes"
  );
  is(
    nodeFront.attributes[0].value,
    classValue,
    "The highlighted node has the correct class"
  );
}
