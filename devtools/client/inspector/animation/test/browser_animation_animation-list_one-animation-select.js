/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the animation item has been selected from first time
// if count of the animations is one.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated"]);
  const { panel } = await openAnimationInspector();

  info("Checking whether an item element has been selected");
  is(panel.querySelector(".animation-item").classList.contains("selected"), true,
     "The animation item should have 'selected' class");

  info("Checking whether the element will be unselected after closing the detail pane");
  clickOnDetailCloseButton(panel);
  is(panel.querySelector(".animation-item").classList.contains("selected"), false,
     "The animation item should not have 'selected' class");
});
