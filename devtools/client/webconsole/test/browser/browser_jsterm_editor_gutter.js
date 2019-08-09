/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that CodeMirror's gutter in console input is displayed when
// 'devtools.webconsole.input.editor' is true.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1519315

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Test JsTerm editor line gutters";

add_task(async function() {
  await pushPref("devtools.webconsole.features.editor", true);
  await pushPref("devtools.webconsole.input.editor", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Check that the line numbers gutter is rendered when in editor layout");
  ok(
    getLineNumbersGutterElement(hud),
    "line numbers gutter is rendered on the input when in editor mode."
  );

  info(
    "Check that the line numbers gutter is hidden we switch to the inline layout"
  );
  await toggleLayout(hud);
  ok(
    !getLineNumbersGutterElement(hud),
    "line numbers gutter is hidden on the input when in inline mode."
  );

  info(
    "Check that the line numbers gutter is rendered again we switch back to editor"
  );
  await toggleLayout(hud);
  ok(
    getLineNumbersGutterElement(hud),
    "line numbers gutter is rendered again on the " +
      " input when switching back to editor mode."
  );
});

function getLineNumbersGutterElement(hud) {
  return hud.ui.outputNode.querySelector(".CodeMirror-linenumbers");
}
