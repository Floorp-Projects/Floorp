/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the panel shows no animation data for invalid or not animated nodes

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".long", ".still"]);
  const { inspector, panel } = await openAnimationInspector();

  info("Checking animation list and error message existence for a still node");
  const stillNode = await getNodeFront(".still", inspector);
  await selectNode(stillNode, inspector);

  await waitUntil(() => panel.querySelector(".animation-error-message"));
  ok(
    true,
    "Element which has animation-error-message class should exist for a still node"
  );
  is(
    panel.querySelector(".animation-error-message > p").textContent,
    ANIMATION_L10N.getStr("panel.noAnimation"),
    "The correct error message is displayed"
  );
  ok(
    !panel.querySelector(".animation-list"),
    "Element which has animations class should not exist for a still node"
  );

  info(
    "Show animations once to confirm if there is no animations on the comment node"
  );
  await selectNode(".long", inspector);
  await waitUntil(() => !panel.querySelector(".animation-error-message"));

  info("Checking animation list and error message existence for a text node");
  const commentNode = await inspector.walker.previousSibling(stillNode);
  await selectNode(commentNode, inspector);
  await waitUntil(() => panel.querySelector(".animation-error-message"));
  ok(
    panel.querySelector(".animation-error-message"),
    "Element which has animation-error-message class should exist for a text node"
  );
  ok(
    !panel.querySelector(".animation-list"),
    "Element which has animations class should not exist for a text node"
  );
});
