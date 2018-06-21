/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the animation items in the list were selectable.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".long"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking whether 1st element will be selected");
  await clickOnAnimation(animationInspector, panel, 0);
  assertSelection(panel, [true, false]);

  info("Checking whether 2nd element will be selected");
  await clickOnAnimation(animationInspector, panel, 1);
  assertSelection(panel, [false, true]);

  info("Checking whether all elements will be unselected after closing the detail pane");
  clickOnDetailCloseButton(panel);
  assertSelection(panel, [false, false]);
});

function assertSelection(panel, expectedResult) {
  panel.querySelectorAll(".animation-item").forEach((item, index) => {
    const shouldSelected = expectedResult[index];
    is(item.classList.contains("selected"), shouldSelected,
       `Animation item[${ index }] should ` +
       `${ shouldSelected ? "" : "not" } have 'selected' class`);
  });
}
