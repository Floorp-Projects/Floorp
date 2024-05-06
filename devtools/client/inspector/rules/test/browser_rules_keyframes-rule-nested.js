/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that nested @keyframes rules are displayed correctly in the rule view

const TEST_URI = `data:text/html,${encodeURIComponent(`
  <style>
    body {
      animation-name: in-layer,
                      in-starting-style,
                      in-media,
                      in-container;
      animation-duration: 1s, 2s, 3s, 4s;
      border: 4px solid;
      outline: 4px solid;
    }

    @layer {
      @keyframes in-layer {
        from { color: red; }
        to   { color: blue; }
      }
    }

    @starting-style {
      /* keyframes is considered as being outside of @starting-style */
      @keyframes in-starting-style {
        from { border-color: tomato; }
        to   { border-color: gold; }
      }
    }

    @media screen {
      @keyframes in-media {
        from { outline-color: purple; }
        to   { outline-color: pink; }
      }
    }

    @container (width > 0px) {
      /* keyframes is considered as being outside of @container */
      @keyframes in-container {
        from { background-color: green; }
        to   { background-color: lime; }
      }
    }
  </style>
  <h1>Nested <code>@keyframes</code></h1>
`)}`;

add_task(async function () {
  await pushPref("layout.css.starting-style-at-rules.enabled", true);
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();

  await selectNode("body", inspector);
  const headers = Array.from(view.element.querySelectorAll(".ruleview-header"));
  Assert.deepEqual(
    headers.map(el => el.textContent),
    [
      "Keyframes in-layer",
      "Keyframes in-starting-style",
      "Keyframes in-media",
      "Keyframes in-container",
    ],
    "Got expected keyframes sections"
  );

  info("Check that keyframes' keyframe ancestor rules are not displayed");
  for (const headerEl of headers) {
    const keyframeContainerId = headerEl
      .querySelector("button")
      .getAttribute("aria-controls");
    const keyframeContainer = view.element.querySelector(
      `#${keyframeContainerId}`
    );
    ok(
      !!keyframeContainer,
      `Got keyframe container for "${headerEl.textContent}"`
    );
    is(
      keyframeContainer.querySelector(".ruleview-rule-ancestor"),
      null,
      `ancestor data are not displayed for "${headerEl.textContent}" keyframe rules`
    );
  }
});
