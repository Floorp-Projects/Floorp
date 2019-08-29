/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following highlighting related.
// * highlight when mouse over on a target node
// * unhighlight when mouse out from the above element
// * lock highlighting when click on the inspect icon in animation target component
// * add 'highlighting' class to animation target component during locking
// * mouseover locked target node
// * unlock highlighting when click on the above icon
// * lock highlighting when click on the other inspect icon
// * if the locked node has multi animations,
//   the class will add to those animation target as well

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".multi"]);
  const {
    animationInspector,
    inspector,
    panel,
  } = await openAnimationInspector();

  info("Check highlighting when mouse over on a target node");
  const onHighlight = inspector.highlighter.once("node-highlight");
  mouseOverOnTargetNode(animationInspector, panel, 0);
  let nodeFront = await onHighlight;
  assertNodeFront(nodeFront, "DIV", "ball animated");

  info("Check unhighlighting when mouse out on a target node");
  const onUnhighlight = inspector.highlighter.once("node-unhighlight");
  mouseOutOnTargetNode(animationInspector, panel, 0);
  await onUnhighlight;
  ok(true, "Unhighlighted the targe node");

  info("Check node is highlighted when the inspect icon is clicked");
  let onHighlighterShown = inspector.highlighters.once(
    "box-model-highlighter-shown"
  );
  await clickOnInspectIcon(animationInspector, panel, 0);
  nodeFront = await onHighlighterShown;
  assertNodeFront(nodeFront, "DIV", "ball animated");
  ok(
    panel
      .querySelectorAll(".animation-target")[0]
      .classList.contains("highlighting"),
    "The highlighted animation target element should have 'highlighting' class"
  );

  info("Check if the animation target is still highlighted on mouse out");
  mouseOutOnTargetNode(animationInspector, panel, 0);
  await wait(500);
  ok(
    panel
      .querySelectorAll(".animation-target")[0]
      .classList.contains("highlighting"),
    "The highlighted element still should have 'highlighting' class"
  );

  info("Check no highlight event occur by mouse over locked target");
  let highlightEventCount = 0;
  const highlightEventCounter = () => {
    highlightEventCount += 1;
  };
  inspector.highlighter.on("node-highlight", highlightEventCounter);
  mouseOverOnTargetNode(animationInspector, panel, 0);
  await wait(500);
  is(highlightEventCount, 0, "Highlight event should not occur");
  inspector.highlighter.off("node-highlight", highlightEventCounter);

  info("Highlighting another animation target");
  onHighlighterShown = inspector.highlighters.once(
    "box-model-highlighter-shown"
  );
  await clickOnInspectIcon(animationInspector, panel, 1);
  nodeFront = await onHighlighterShown;
  assertNodeFront(nodeFront, "DIV", "ball multi");

  info("Check the highlighted state of the animation targets");
  const animationTargetEls = panel.querySelectorAll(".animation-target");
  ok(
    !animationTargetEls[0].classList.contains("highlighting"),
    "The animation target[0] should not have 'highlighting' class"
  );
  ok(
    animationTargetEls[1].classList.contains("highlighting"),
    "The animation target[1] should have 'highlighting' class"
  );
  ok(
    animationTargetEls[2].classList.contains("highlighting"),
    "The animation target[2] should have 'highlighting' class"
  );

  info("Hide highlighter");
  const onHighlighterHidden = inspector.highlighters.once(
    "box-model-highlighter-hidden"
  );
  await clickOnInspectIcon(animationInspector, panel, 1);
  await onHighlighterHidden;

  info("Check the highlighted state of the animation targets");
  ok(
    !animationTargetEls[1].classList.contains("highlighting"),
    "The animation target[1] should not have 'highlighting' class"
  );
  ok(
    !animationTargetEls[2].classList.contains("highlighting"),
    "The animation target[2] should not have 'highlighting' class"
  );
});

function assertNodeFront(nodeFront, tagName, classValue) {
  is(nodeFront.tagName, "DIV", "The highlighted node has the correct tagName");
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
