/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that basic keyboard shortcuts work in the web console.

"use strict";

add_task(async function () {
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/old/" +
                   "test/test-console.html";

  await loadTab(TEST_URI);

  let hud = await openConsole();
  ok(hud, "Web Console opened");

  info("dump some spew into the console for scrolling");
  hud.jsterm.execute("(function() { for (var i = 0; i < 100; i++) { " +
                     "console.log('foobarz' + i);" +
                     "}})();");

  await waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foobarz99",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  let currentPosition = hud.ui.outputWrapper.scrollTop;
  let bottom = currentPosition;

  EventUtils.synthesizeKey("KEY_PageUp");
  isnot(hud.ui.outputWrapper.scrollTop, currentPosition,
        "scroll position changed after page up");

  currentPosition = hud.ui.outputWrapper.scrollTop;
  EventUtils.synthesizeKey("KEY_PageDown");
  ok(hud.ui.outputWrapper.scrollTop > currentPosition,
     "scroll position now at bottom");

  EventUtils.synthesizeKey("KEY_Home");
  is(hud.ui.outputWrapper.scrollTop, 0, "scroll position now at top");

  EventUtils.synthesizeKey("KEY_End");

  let scrollTop = hud.ui.outputWrapper.scrollTop;
  ok(scrollTop > 0 && Math.abs(scrollTop - bottom) <= 5,
     "scroll position now at bottom");

  info("try ctrl-l to clear output");
  executeSoon(() => {
    let clearShortcut;
    if (Services.appinfo.OS === "Darwin") {
      clearShortcut = WCUL10n.getStr("webconsole.clear.keyOSX");
    } else {
      clearShortcut = WCUL10n.getStr("webconsole.clear.key");
    }
    synthesizeKeyShortcut(clearShortcut);
  });
  await hud.jsterm.once("messages-cleared");

  // Wait for the next event tick to make sure keyup for the shortcut above
  // finishes.  Otherwise the 2 shortcuts are mixed.
  await new Promise(executeSoon);

  is(hud.outputNode.textContent.indexOf("foobarz1"), -1, "output cleared");
  is(hud.jsterm.inputNode.getAttribute("focused"), "true",
     "jsterm input is focused");

  info("try ctrl-f to focus filter");
  synthesizeKeyShortcut(WCUL10n.getStr("webconsole.find.key"));
  ok(!hud.jsterm.inputNode.getAttribute("focused"),
     "jsterm input is not focused");
  is(hud.ui.filterBox.getAttribute("focused"), "true",
     "filter input is focused");
});
