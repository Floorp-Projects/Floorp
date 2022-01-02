/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that hovering over a box model value will jump to its source CSS rule in the
// rules view when the shift key is pressed.

const TEST_URI = `<style>
    #box {
      margin: 5px;
    }
  </style>
  <div id="box"></div>`;

add_task(async function() {
  await pushPref("devtools.layout.boxmodel.highlightProperty", true);
  await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const { inspector, boxmodel } = await openLayoutView();
  await selectNode("#box", inspector);

  info(
    "Test that hovering over margin-top value highlights the property in rules view."
  );
  const ruleView = await inspector.getPanel("ruleview").view;
  const el = boxmodel.document.querySelector(
    ".boxmodel-margin.boxmodel-top > span"
  );

  info("Wait for mouse to hover over margin-top element.");
  const onHighlightProperty = ruleView.once("scrolled-to-element");
  EventUtils.synthesizeMouseAtCenter(
    el,
    { type: "mousemove", shiftKey: true },
    boxmodel.document.defaultView
  );
  await onHighlightProperty;

  info("Check that margin-top is visible in the rule view.");
  const { rules, styleWindow } = ruleView;
  const marginTop = rules[1].textProps[0].computed[0];
  ok(
    isInViewport(marginTop.element, styleWindow),
    "margin-top is visible in the rule view."
  );
});

function isInViewport(element, win) {
  const { top, left, bottom, right } = element.getBoundingClientRect();
  return (
    top >= 0 &&
    bottom <= win.innerHeight &&
    left >= 0 &&
    right <= win.innerWidth
  );
}
