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

add_task(async function () {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".multi"]);
  const { animationInspector, inspector, panel } =
    await openAnimationInspector();
  const { waitForHighlighterTypeShown, waitForHighlighterTypeHidden } =
    getHighlighterTestHelpers(inspector);

  info("Check highlighting when mouse over on a target node");
  const onHighlight = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );
  mouseOverOnTargetNode(animationInspector, panel, 0);
  let data = await onHighlight;
  assertNodeFront(data.nodeFront, "DIV", "ball animated");

  info("Check unhighlighting when mouse out on a target node");
  const onUnhighlight = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.BOXMODEL
  );
  mouseOutOnTargetNode(animationInspector, panel, 0);
  await onUnhighlight;
  ok(true, "Unhighlighted the targe node");

  info("Check node is highlighted when the inspect icon is clicked");
  const onHighlighterShown = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );
  await clickOnInspectIcon(animationInspector, panel, 0);
  data = await onHighlighterShown;
  assertNodeFront(data.nodeFront, "DIV", "ball animated");
  await assertHighlight(panel, 0, true);

  info("Check if the animation target is still highlighted on mouse out");
  mouseOutOnTargetNode(animationInspector, panel, 0);
  await wait(500);
  await assertHighlight(panel, 0, true);

  info("Check no highlight event occur by mouse over locked target");
  let highlightEventCount = 0;
  function onHighlighterHidden({ type }) {
    if (type === inspector.highlighters.TYPES.BOXMODEL) {
      highlightEventCount += 1;
    }
  }
  inspector.highlighters.on("highlighter-hidden", onHighlighterHidden);
  mouseOverOnTargetNode(animationInspector, panel, 0);
  await wait(500);
  is(highlightEventCount, 0, "Highlight event should not occur");
  inspector.highlighters.off("highlighter-hidden", onHighlighterHidden);

  info("Show persistent highlighter on an animation target");
  const onPersistentHighlighterShown = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.SELECTOR
  );
  await clickOnInspectIcon(animationInspector, panel, 1);
  data = await onPersistentHighlighterShown;
  assertNodeFront(data.nodeFront, "DIV", "ball multi");

  info("Check the highlighted state of the animation targets");
  await assertHighlight(panel, 0, false);
  await assertHighlight(panel, 1, true);
  await assertHighlight(panel, 2, true);

  info("Hide persistent highlighter");
  const onPersistentHighlighterHidden = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.SELECTOR
  );
  await clickOnInspectIcon(animationInspector, panel, 1);
  await onPersistentHighlighterHidden;

  info("Check the highlighted state of the animation targets");
  await assertHighlight(panel, 0, false);
  await assertHighlight(panel, 1, false);
  await assertHighlight(panel, 2, false);
});

async function assertHighlight(panel, index, isHighlightExpected) {
  const animationItemEl = await findAnimationItemByIndex(panel, index);
  const animationTargetEl = animationItemEl.querySelector(".animation-target");

  await waitUntil(
    () =>
      animationTargetEl.classList.contains("highlighting") ===
      isHighlightExpected
  );
  ok(true, `Highlighting class of animation target[${index}] is correct`);
}

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
