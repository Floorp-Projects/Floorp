/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that basic keyboard shortcuts work in the web console.

"use strict";

const TEST_URI =
  `data:text/html;charset=utf-8,<p>Test keyboard accessibility</p>
  <script>
    for (let i = 1; i <= 100; i++) {
      console.log("console message " + i);
    }
  </script>
  `;

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);
  info("Web Console opened");

  const outputScroller = hud.ui.outputScroller;

  yield waitFor(() => findMessages(hud, "").length == 100);

  let currentPosition = outputScroller.scrollTop;
  const bottom = currentPosition;

  hud.jsterm.inputNode.focus();

  // Page up.
  EventUtils.synthesizeKey("VK_PAGE_UP", {});
  isnot(outputScroller.scrollTop, currentPosition,
    "scroll position changed after page up");

  // Page down.
  currentPosition = outputScroller.scrollTop;
  EventUtils.synthesizeKey("VK_PAGE_DOWN", {});
  ok(outputScroller.scrollTop > currentPosition,
     "scroll position now at bottom");

  // Home
  EventUtils.synthesizeKey("VK_HOME", {});
  is(outputScroller.scrollTop, 0, "scroll position now at top");

  // End
  EventUtils.synthesizeKey("VK_END", {});
  let scrollTop = outputScroller.scrollTop;
  ok(scrollTop > 0 && Math.abs(scrollTop - bottom) <= 5,
     "scroll position now at bottom");

  // Clear output
  info("try ctrl-l to clear output");
  let clearShortcut;
  if (Services.appinfo.OS === "Darwin") {
    clearShortcut = WCUL10n.getStr("webconsole.clear.keyOSX");
  } else {
    clearShortcut = WCUL10n.getStr("webconsole.clear.key");
  }
  synthesizeKeyShortcut(clearShortcut);
  yield waitFor(() => findMessages(hud, "").length == 0);
  is(hud.jsterm.inputNode.getAttribute("focused"), "true", "jsterm input is focused");

  // Focus filter
  info("try ctrl-f to focus filter");
  synthesizeKeyShortcut(WCUL10n.getStr("webconsole.find.key"));
  ok(!hud.jsterm.inputNode.getAttribute("focused"), "jsterm input is not focused");
  is(hud.ui.filterBox, outputScroller.ownerDocument.activeElement,
    "filter input is focused");
});
