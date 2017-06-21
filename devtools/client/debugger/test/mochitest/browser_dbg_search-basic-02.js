/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests basic file search functionality.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

var gTab, gPanel, gDebugger;
var gSources, gSearchBox;

function test() {
  let options = {
    source: EXAMPLE_URL + "code_script-switching-01.js",
    line: 1,
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    // Calling `firstCall` is going to break into the other script
    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 6)
      .then(performSimpleSearch)
      .then(() => verifySourceAndCaret("-01.js", 1, 1, [1, 1]))
      .then(combineWithLineSearch)
      .then(() => verifySourceAndCaret("-01.js", 2, 1, [53, 53]))
      .then(combineWithTokenSearch)
      .then(() => verifySourceAndCaret("-01.js", 2, 48, [96, 100]))
      .then(combineWithTokenColonSearch)
      .then(() => verifySourceAndCaret("-01.js", 2, 11, [56, 63]))
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "firstCall");
  });
}

function performSimpleSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-02.js"),
    ensureCaretAt(gPanel, 6),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForSourceShown(gPanel, "-01.js")
  ]);

  setText(gSearchBox, "1");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1)
  ]));
}

function combineWithLineSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForCaretUpdated(gPanel, 2)
  ]);

  typeText(gSearchBox, ":2");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 2)
  ]));
}

function combineWithTokenSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 2),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForCaretUpdated(gPanel, 2, 48)
  ]);

  backspaceText(gSearchBox, 2);
  typeText(gSearchBox, "#zero");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 2, 48)
  ]));
}

function combineWithTokenColonSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 2, 48),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForCaretUpdated(gPanel, 2, 11)
  ]);

  backspaceText(gSearchBox, 4);
  typeText(gSearchBox, "http://");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 2, 11)
  ]));
}

function verifySourceAndCaret(aUrl, aLine, aColumn, aSelection) {
  ok(gSources.selectedItem.attachment.label.includes(aUrl),
    "The selected item's label appears to be correct.");
  ok(gSources.selectedItem.attachment.source.url.includes(aUrl),
    "The selected item's value appears to be correct.");
  ok(isCaretPos(gPanel, aLine, aColumn),
    "The current caret position appears to be correct.");
  ok(isEditorSel(gPanel, aSelection),
    "The current editor selection appears to be correct.");
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
  gSearchBox = null;
});
