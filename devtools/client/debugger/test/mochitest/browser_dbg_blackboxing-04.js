/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get a stack frame for each black boxed source, not a single one
 * for all of them.
 */

const TAB_URL = EXAMPLE_URL + "doc_blackboxing.html";
const BLACKBOXME_URL = EXAMPLE_URL + "code_blackboxing_blackboxme.js";

var gTab, gPanel, gDebugger;
var gFrames, gSources;

function test() {
  let options = {
    source: BLACKBOXME_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gSources = gDebugger.DebuggerView.Sources;

    blackBoxSources()
      .then(testBlackBoxStack)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function blackBoxSources() {
  let finished = waitForThreadEvents(gPanel, "blackboxchange", 3);

  toggleBlackBoxing(gPanel, getSourceActor(gSources, EXAMPLE_URL + "code_blackboxing_one.js"));
  toggleBlackBoxing(gPanel, getSourceActor(gSources, EXAMPLE_URL + "code_blackboxing_two.js"));
  toggleBlackBoxing(gPanel, getSourceActor(gSources, EXAMPLE_URL + "code_blackboxing_three.js"));
  return finished;
}

function testBlackBoxStack() {
  let finished = waitForSourceAndCaretAndScopes(gPanel, ".html", 21).then(() => {
    is(gFrames.itemCount, 4,
      "Should get 4 frames (one -> two -> three -> doDebuggerStatement).");
    is(gDebugger.document.querySelectorAll(".dbg-stackframe-black-boxed").length, 3,
      "And 'one', 'two', and 'three' should each have their own black boxed frame.");
  });

  callInTab(gTab, "one");
  return finished;
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gFrames = null;
  gSources = null;
});
