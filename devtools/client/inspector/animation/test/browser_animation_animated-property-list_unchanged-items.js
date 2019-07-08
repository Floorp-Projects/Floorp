/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the position and the class of unchanged animated property items.

const TEST_DATA = [
  { property: "background-color", isUnchanged: false },
  { property: "padding-left", isUnchanged: false },
  { property: "background-attachment", isUnchanged: true },
  { property: "background-clip", isUnchanged: true },
  { property: "background-image", isUnchanged: true },
  { property: "background-origin", isUnchanged: true },
  { property: "background-position-x", isUnchanged: true },
  { property: "background-position-y", isUnchanged: true },
  { property: "background-repeat", isUnchanged: true },
  { property: "background-size", isUnchanged: true },
  { property: "padding-bottom", isUnchanged: true },
  { property: "padding-right", isUnchanged: true },
  { property: "padding-top", isUnchanged: true },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".longhand"]);
  const { panel } = await openAnimationInspector();

  info("Checking unchanged animated property item");
  const itemEls = panel.querySelectorAll(".animated-property-item");
  is(
    itemEls.length,
    TEST_DATA.length,
    `Count of animated property item should be ${TEST_DATA.length}`
  );

  for (let i = 0; i < TEST_DATA.length; i++) {
    const { property, isUnchanged } = TEST_DATA[i];
    const itemEl = itemEls[i];

    ok(
      itemEl.querySelector(`.keyframes-graph.${property}`),
      `Item of ${property} should display at here`
    );

    if (isUnchanged) {
      ok(
        itemEl.classList.contains("unchanged"),
        "Animated property item should have 'unchanged' class"
      );
    } else {
      ok(
        !itemEl.classList.contains("unchanged"),
        "Animated property item should not have 'unchanged' class"
      );
    }
  }
});
