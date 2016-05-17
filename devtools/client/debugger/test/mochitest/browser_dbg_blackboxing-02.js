/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that black boxed frames are compressed into a single frame on the stack
 * view.
 */

const TAB_URL = EXAMPLE_URL + "doc_blackboxing.html";
const BLACKBOXME_URL = EXAMPLE_URL + "code_blackboxing_blackboxme.js";

var gTab, gPanel, gDebugger;
var gFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;

    waitForSourceShown(gPanel, BLACKBOXME_URL)
      .then(testBlackBoxSource)
      .then(testBlackBoxStack)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testBlackBoxSource() {
  return toggleBlackBoxing(gPanel).then(source => {
    ok(source.isBlackBoxed, "The source should be black boxed now.");
  });
}

function testBlackBoxStack() {
  let finished = waitForSourceAndCaretAndScopes(gPanel, ".html", 21).then(() => {
    is(gFrames.itemCount, 3,
      "Should only get 3 frames.");
    is(gDebugger.document.querySelectorAll(".dbg-stackframe-black-boxed").length, 1,
      "And one of them should be the combined black boxed frames.");
  });

  callInTab(gTab, "runTest");
  return finished;
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gFrames = null;
});
