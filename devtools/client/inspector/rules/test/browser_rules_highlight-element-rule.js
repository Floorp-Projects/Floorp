/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view's highlightElementRule scrolls to the specified rule.

const TEST_URI = `
  <style type="text/css">
    .test::after {
      content: "!";
      color: red;
    }
  </style>
  <div class="test">Hello</div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await selectNode(".test", inspector);
  const { rules, styleWindow } = view;

  info("Highlight .test::after rule.");
  const ruleId = rules[0].domRule.actorID;

  info("Wait for the view to scroll to the property.");
  const onHighlightProperty = view.once("scrolled-to-element");

  view.highlightElementRule(ruleId);

  await onHighlightProperty;

  ok(
    isInViewport(rules[0].editor.element, styleWindow),
    ".test::after is in view."
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
