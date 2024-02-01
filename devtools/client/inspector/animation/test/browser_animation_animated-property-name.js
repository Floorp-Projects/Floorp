/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the following animated property name component features:
// * name of property
// * display compositor sign when the property was running on compositor.
// * display warning when the property is runnable on compositor but was not.

async function test_element(className, data) {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([className]);
  const { panel } = await openAnimationInspector();

  info("Checking animated property name component");
  const animatedPropertyNameEls = panel.querySelectorAll(
    ".animated-property-name"
  );
  is(
    animatedPropertyNameEls.length,
    data.length,
    `Number of animated property name elements should be ${data.length}`
  );

  for (const [
    index,
    animatedPropertyNameEl,
  ] of animatedPropertyNameEls.entries()) {
    const { property, isOnCompositor, isWarning } = data[index];

    info(`Checking text content for ${property}`);

    const spanEl = animatedPropertyNameEl.querySelector("span");
    ok(
      spanEl,
      `<span> element should be in animated-property-name of ${property}`
    );
    is(spanEl.textContent, property, `textContent should be ${property}`);

    info(`Checking compositor sign for ${property}`);

    if (isOnCompositor) {
      ok(
        animatedPropertyNameEl.classList.contains("compositor"),
        "animatedPropertyNameEl should has .compositor class"
      );
      isnot(
        getComputedStyle(spanEl, "::before").width,
        "auto",
        "width of ::before pseud should not be auto"
      );
    } else {
      ok(
        !animatedPropertyNameEl.classList.contains("compositor"),
        "animatedPropertyNameEl should not have .compositor class"
      );
      is(
        getComputedStyle(spanEl, "::before").width,
        "auto",
        "width of ::before pseud should be auto"
      );
    }

    info(`Checking warning for ${property}`);

    if (isWarning) {
      ok(
        animatedPropertyNameEl.classList.contains("warning"),
        "animatedPropertyNameEl should has .warning class"
      );
      is(
        getComputedStyle(spanEl).textDecorationStyle,
        "dotted",
        "text-decoration-style of spanEl should be 'dotted'"
      );
      is(
        getComputedStyle(spanEl).textDecorationLine,
        "underline",
        "text-decoration-line of spanEl should be 'underline'"
      );
    } else {
      ok(
        !animatedPropertyNameEl.classList.contains("warning"),
        "animatedPropertyNameEl should not have .warning class"
      );
      is(
        getComputedStyle(spanEl).textDecorationStyle,
        "solid",
        "text-decoration-style of spanEl should be 'solid'"
      );
      is(
        getComputedStyle(spanEl).textDecorationLine,
        "none",
        "text-decoration-line of spanEl should be 'none'"
      );
    }
  }
}

add_task(async function compositor_notall() {
  await test_element(".compositor-notall", [
    {
      property: "--ball-color",
    },
    {
      property: "opacity",
      isOnCompositor: true,
    },
    {
      property: "transform",
      isOnCompositor: true,
    },
    {
      property: "width",
    },
  ]);
});

add_task(async function compositor_warning() {
  await test_element(".compositor-warning", [
    {
      property: "opacity",
      isWarning: true,
    },
  ]);
});
