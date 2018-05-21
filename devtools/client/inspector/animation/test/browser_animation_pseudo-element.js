/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for pseudo element.

const TEST_DATA = [
  {
    expectedTargetLabel: "::before",
    expectedAnimationNameLabel: "body",
  },
  {
    expectedTargetLabel: "::before",
    expectedAnimationNameLabel: "div-before",
  },
  {
    expectedTargetLabel: "::after",
    expectedAnimationNameLabel: "div-after",
  },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_pseudo.html");
  const { animationInspector, inspector, panel } = await openAnimationInspector();

  info("Checking count of animation item for pseudo elements");
  is(panel.querySelectorAll(".animation-list .animation-item").length, TEST_DATA.length,
    `Count of animation item should be ${ TEST_DATA.length }`);

  info("Checking content of each animation item");
  const animationItemEls = panel.querySelectorAll(".animation-list .animation-item");

  for (let i = 0; i < animationItemEls.length; i++) {
    const testData = TEST_DATA[i];
    info(`Checking pseudo element for ${ testData.expectedTargetLabel }`);
    const animationItemEl = animationItemEls[i];

    info("Checking text content of animation target");
    const animationTargetEl =
      animationItemEl.querySelector(".animation-list .animation-item .animation-target");
    is(animationTargetEl.textContent, testData.expectedTargetLabel,
      `Text content of animation target[${ i }] should be ${ testData.expectedTarget }`);

    info("Checking text content of animation name");
    const animationNameEl = animationItemEl.querySelector(".animation-name");
    is(animationNameEl.textContent, testData.expectedAnimationNameLabel,
      `The animation name should be ${ testData.expectedAnimationNameLabel }`);
  }

  info("Checking whether node is selected correctly " +
       "when click on the first inspector icon on Reps component");
  await clickOnTargetNode(animationInspector, panel, 0);

  info("Checking count of animation item");
  is(panel.querySelectorAll(".animation-list .animation-item").length, 1,
    "Count of animation item should be 1");

  info("Checking content of animation item");
  is(panel.querySelector(".animation-list .animation-item .animation-name").textContent,
     TEST_DATA[0].expectedAnimationNameLabel,
     `The animation name should be ${ TEST_DATA[0].expectedAnimationNameLabel }`);

  info("Select <body> again to reset the animation list");
  await selectNodeAndWaitForAnimations("body", inspector);

  info("Checking whether node is selected correctly " +
       "when click on the second inspector icon on Reps component");
  await clickOnTargetNode(animationInspector, panel, 1);

  info("Checking count of animation item");
  is(panel.querySelectorAll(".animation-list .animation-item").length, 1,
     "Count of animation item should be 1");

  info("Checking content of animation item");
  is(panel.querySelector(".animation-list .animation-item .animation-name").textContent,
     TEST_DATA[1].expectedAnimationNameLabel,
     `The animation name should be ${ TEST_DATA[1].expectedAnimationNameLabel }`);
});
