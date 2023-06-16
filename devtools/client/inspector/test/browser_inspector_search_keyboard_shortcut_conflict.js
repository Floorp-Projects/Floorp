/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the inspector search shortcut works from the inner iframes of the
// inspector panel (eg markup-view iframe) and that shortcuts triggered by other
// panels are not consumed by the inspector.
// See Bug 1589617.
add_task(async function () {
  const { inspector, toolbox } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<span>Test search shortcut conflicts</span>"
  );
  const { searchBox } = inspector;
  const doc = inspector.panelDoc;

  info("Check that the shortcut works when opening the inspector");
  await focusSearchBoxUsingShortcut(inspector.panelWin);
  ok(containsFocus(doc, searchBox), "Focus is in a searchbox");

  info("Focus the markup view");
  inspector.markup._frame.focus();
  ok(!containsFocus(doc, searchBox), "Focus is no longer in the searchbox");

  info("Check that the shortcut works from the markup view");
  const focused = once(searchBox, "focus");
  synthesizeKeyShortcut(INSPECTOR_L10N.getStr("inspector.searchHTML.key"));
  await focused;
  ok(containsFocus(doc, searchBox), "Focus is in the searchbox again");

  // We focus the markup view again to check if using the shortcut from the
  // webconsole will focus the inspector searchbox unintentionally.
  inspector.markup._frame.focus();
  ok(!containsFocus(doc, searchBox), "Focus is no longer in the searchbox");

  info("Switch to webconsole");
  await toolbox.selectTool("webconsole");
  const hud = toolbox.getCurrentPanel().hud;
  const consoleSearchBox = hud.ui.outputNode.querySelector(
    ".devtools-searchbox input"
  );

  info("Check that the console search shortcut works");
  const consoleSearchFocused = once(consoleSearchBox, "focus");

  // Note: we expect the console and inspector to share the same shortcut.
  // If they diverge, the test will need to be updated.
  synthesizeKeyShortcut(INSPECTOR_L10N.getStr("inspector.searchHTML.key"));
  await consoleSearchFocused;
  const consoleDoc = hud.ui.outputNode.ownerDocument;
  ok(
    containsFocus(consoleDoc, consoleSearchBox),
    "Focus is in the console searchbox"
  );

  info("Switch back to the inspector");
  await toolbox.selectTool("inspector");
  ok(!containsFocus(doc, searchBox), "Focus is not in the inspector searchbox");
});
