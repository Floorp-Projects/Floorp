/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that searches which cause a popup to be shown properly handle the
 * ESCAPE key.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

var gTab, gPanel, gDebugger;
var gSources, gSearchBox;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1)
      .then(performFileSearch)
      .then(escapeAndHide)
      .then(escapeAndClear)
      .then(() => verifySourceAndCaret("-01.js", 1, 1))
      .then(performFunctionSearch)
      .then(escapeAndHide)
      .then(escapeAndClear)
      .then(() => verifySourceAndCaret("-01.js", 4, 10))
      .then(performGlobalSearch)
      .then(escapeAndClear)
      .then(() => verifySourceAndCaret("-01.js", 4, 10))
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "firstCall");
  });
}

function performFileSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-02.js"),
    ensureCaretAt(gPanel, 6),
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForSourceShown(gPanel, "-01.js")
  ]);

  setText(gSearchBox, ".");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1)
  ]));
}

function performFunctionSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FUNCTION_SEARCH_MATCH_FOUND)
  ]);

  setText(gSearchBox, "@");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 4, 10)
  ]));
}

function performGlobalSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 4, 10),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.GLOBAL_SEARCH_MATCH_FOUND)
  ]);

  setText(gSearchBox, "!first");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 4, 10)
  ]));
}

function escapeAndHide() {
  let finished = once(gDebugger, "popuphidden", true);
  EventUtils.sendKey("ESCAPE", gDebugger);
  return finished;
}

function escapeAndClear() {
  EventUtils.sendKey("ESCAPE", gDebugger);
  is(gSearchBox.getAttribute("value"), "",
    "The searchbox has properly emptied after pressing escape.");
}

function verifySourceAndCaret(aUrl, aLine, aColumn) {
  ok(gSources.selectedItem.attachment.label.includes(aUrl),
    "The selected item's label appears to be correct.");
  ok(gSources.selectedItem.attachment.source.url.includes(aUrl),
    "The selected item's value appears to be correct.");
  ok(isCaretPos(gPanel, aLine, aColumn),
    "The current caret position appears to be correct.");
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
  gSearchBox = null;
});
