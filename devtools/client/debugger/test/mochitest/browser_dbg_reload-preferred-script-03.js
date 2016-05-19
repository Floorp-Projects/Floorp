/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the preferred source is shown when a page is loaded and
 * the preferred source is specified after another source was definitely shown.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";
const FIRST_URL = EXAMPLE_URL + "code_script-switching-01.js";
const SECOND_URL = EXAMPLE_URL + "code_script-switching-02.js";

var gTab, gPanel, gDebugger;
var gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceShown(gPanel, FIRST_URL)
      .then(() => testSource(undefined, FIRST_URL))
      .then(() => switchToSource(SECOND_URL))
      .then(() => testSource(SECOND_URL))
      .then(() => switchToSource(FIRST_URL))
      .then(() => testSource(FIRST_URL))
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testSource(aPreferredUrl, aSelectedUrl = aPreferredUrl) {
  info("Currently preferred source: " + gSources.preferredValue);
  info("Currently selected source: " + gSources.selectedValue);

  is(getSourceURL(gSources, gSources.preferredValue), aPreferredUrl,
    "The preferred source url wasn't set correctly.");
  is(getSourceURL(gSources, gSources.selectedValue), aSelectedUrl,
    "The selected source isn't the correct one.");
}

function switchToSource(aUrl) {
  let finished = waitForSourceShown(gPanel, aUrl);
  gSources.preferredSource = getSourceActor(gSources, aUrl);
  return finished;
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
});
