/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter is hidden when selecting frames in the iframe picker

const TEST_URI = `
  <style type="text/css">
    body {
      background: red;
    }
  </style>
  <h1>Test the selector highlighter</h1>
  <iframe src="data:text/html,<meta charset=utf8><style>h2 {background: yellow;}</style><h2>In iframe</h2>">
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, toolbox, view } = await openRuleView();

  info("Clicking on a selector icon");
  const { highlighter, isShown } = await clickSelectorIcon(view, "body");

  ok(highlighter, "The selector highlighter instance was created");
  ok(isShown, "The selector highlighter was shown");
  is(
    highlighter,
    inspector.highlighters.getActiveHighlighter(
      inspector.highlighters.TYPES.SELECTOR
    ),
    "The selector highlighter is the active highlighter"
  );

  // Open frame menu and wait till it's available on the screen.
  const panel = toolbox.doc.getElementById("command-button-frames-panel");
  const btn = toolbox.doc.getElementById("command-button-frames");
  btn.click();
  ok(panel, "popup panel has created.");
  await waitUntil(() => panel.classList.contains("tooltip-visible"));

  // Verify that the menu is populated.
  const menuList = toolbox.doc.getElementById("toolbox-frame-menu");
  const frames = Array.from(menuList.querySelectorAll(".command"));

  // Wait for the inspector to be reloaded
  // (instead of only new-root) in order to wait for full
  // async update of the inspector.
  const onNewRoot = inspector.once("reloaded");
  frames[1].click();
  await onNewRoot;

  await waitFor(
    () =>
      !inspector.highlighters.getActiveHighlighter(
        inspector.highlighters.TYPES.SELECTOR
      )
  );
  ok(true, "The selector highlighter gets hidden after selecting a frame");
});
