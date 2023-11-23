/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter is created when clicking on a selector
// icon in the rule view.

const TEST_URI = `
  <style type="text/css">
    body, p, td {
      background: red;
    }
  </style>
  Test the selector highlighter
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  const { waitForHighlighterTypeShown, waitForHighlighterTypeHidden } =
    getHighlighterTestHelpers(inspector);

  const activeHighlighter = inspector.highlighters.getActiveHighlighter(
    inspector.highlighters.TYPES.SELECTOR
  );
  ok(!activeHighlighter, "No selector highlighter is active");

  info("Clicking on a selector icon");
  let data = await clickSelectorIcon(view, "body, p, td");

  ok(data.highlighter, "The selector highlighter instance was created");
  ok(data.isShown, "The selector highlighter was shown");

  info("Click on the same icon to disable highlighter");
  data = await clickSelectorIcon(view, "body, p, td");
  ok(!data.isShown, "The highlighter is not visible anymore");

  info("Check that the selector highlighter can be toggled from the keyboard");
  const ruleEl = getRuleViewRule(view, "body, p, td", 0);
  const selectorContainerEl = ruleEl.querySelector(
    ".ruleview-selectors-container"
  );
  const selectorHighlighterIcon = ruleEl.querySelector(
    ".ruleview-selectorhighlighter"
  );
  is(
    selectorHighlighterIcon.getAttribute("aria-pressed"),
    "false",
    "selector highlighter icon is not pressed by default"
  );
  selectorContainerEl.focus();
  EventUtils.synthesizeKey("VK_TAB", {}, selectorContainerEl.ownerGlobal);
  await waitFor(
    () =>
      selectorContainerEl.ownerDocument.activeElement ===
      selectorHighlighterIcon
  );
  ok(true, "selector highlighter button can be focused");

  const onHighlighterShown = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.SELECTOR
  );
  EventUtils.synthesizeKey("VK_RETURN", {}, selectorContainerEl.ownerGlobal);
  data = await onHighlighterShown;

  ok(true, "The selector highlighter was shown from the keyboard");
  ok(data.highlighter, "The selector highlighter instance was created");

  await waitFor(
    () => selectorHighlighterIcon.getAttribute("aria-pressed") === "true"
  );
  ok(true, "selector highlighter icon is pressed");

  const onHighlighterHidden = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.SELECTOR
  );
  EventUtils.synthesizeKey("VK_RETURN", {}, selectorContainerEl.ownerGlobal);
  await onHighlighterHidden;
  ok(true, "The selector highlighter was hidden from the keyboard");
  is(
    selectorHighlighterIcon.getAttribute("aria-pressed"),
    "false",
    "selector highlighter icon is no longer pressed"
  );
});
