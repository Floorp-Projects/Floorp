/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for code folding appears in editor mode, does not appear in inline mode,
// and that folded code does not remain folded when switched to inline mode.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1581641

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html>Test JsTerm editor code folding";

add_task(async function () {
  await pushPref("devtools.webconsole.input.editor", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Check that code folding gutter & arrow are rendered in editor mode");

  const multilineExpression = `function() {
    // Silence is golden
  }`;
  await setInputValue(hud, multilineExpression);

  ok(
    await waitFor(() => getFoldArrowOpenElement(hud)),
    "code folding gutter was rendered in editor mode"
  );

  const foldingArrow = getFoldArrowOpenElement(hud);
  ok(foldingArrow, "code folding arrow was rendered in code folding gutter");
  is(getCodeLines(hud).length, 3, "There are 3 lines displayed");

  info("Check that code folds when gutter marker clicked");
  EventUtils.synthesizeMouseAtCenter(
    foldingArrow,
    {},
    foldingArrow.ownerDocument.defaultView
  );
  await waitFor(() => getCodeLines(hud).length === 1);
  ok(true, "The code was folded, there's only one line displayed now");

  info("Check that folded code is expanded when rendered inline");

  await toggleLayout(hud);

  is(
    getCodeLines(hud).length,
    3,
    "folded code is expended when rendered in inline"
  );

  info(
    "Check that code folding gutter is hidden when we switch to inline mode"
  );
  ok(
    !getFoldGutterElement(hud),
    "code folding gutter is hidden when we switsch to inline mode"
  );
});

function getCodeLines(hud) {
  return hud.ui.outputNode.querySelectorAll(
    ".CodeMirror-code pre.CodeMirror-line"
  );
}

function getFoldGutterElement(hud) {
  return hud.ui.outputNode.querySelector(".CodeMirror-foldgutter");
}

function getFoldArrowOpenElement(hud) {
  return hud.ui.outputNode.querySelector(".CodeMirror-foldgutter-open");
}
