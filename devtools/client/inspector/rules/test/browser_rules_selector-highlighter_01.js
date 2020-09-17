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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  const activeHighlighter = inspector.highlighters.getActiveHighlighter(
    inspector.highlighters.TYPES.SELECTOR
  );
  ok(!activeHighlighter, "No selector highlighter is active");

  info("Clicking on a selector icon");
  const { highlighter, isShown } = await clickSelectorIcon(view, "body, p, td");

  ok(highlighter, "The selector highlighter instance was created");
  ok(isShown, "The selector highlighter was shown");
});
