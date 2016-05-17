/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests more complex functionality of sources filtering (file search).
 */

const TAB_URL = EXAMPLE_URL + "doc_editor-mode.html";

var gTab, gPanel, gDebugger;
var gSources, gSourceUtils, gSearchView, gSearchBox;

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(3);

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gSourceUtils = gDebugger.SourceUtils;
    gSearchView = gDebugger.DebuggerView.Filtering.FilteredSources;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    waitForSourceShown(gPanel, "-01.js")
      .then(firstSearch)
      .then(secondSearch)
      .then(thirdSearch)
      .then(fourthSearch)
      .then(fifthSearch)
      .then(goDown)
      .then(goDownAndWrap)
      .then(goUpAndWrap)
      .then(goUp)
      .then(returnAndSwitch)
      .then(firstSearch)
      .then(clickAndSwitch)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function firstSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND)
  ]);

  setText(gSearchBox, ".");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    verifyContents([
      "code_script-switching-01.js?a=b",
      "code_test-editor-mode?c=d",
      "doc_editor-mode.html"
    ])
  ]));
}

function secondSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND)
  ]);

  typeText(gSearchBox, "-0");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    verifyContents(["code_script-switching-01.js?a=b"])
  ]));
}

function thirdSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND)
  ]);

  backspaceText(gSearchBox, 1);

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    verifyContents([
      "code_script-switching-01.js?a=b",
      "code_test-editor-mode?c=d",
      "doc_editor-mode.html"
    ])
  ]));
}

function fourthSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForSourceShown(gPanel, "test-editor-mode")
  ]);

  setText(gSearchBox, "code_test");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "test-editor-mode"),
    ensureCaretAt(gPanel, 1),
    verifyContents(["code_test-editor-mode?c=d"])
  ]));
}

function fifthSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "test-editor-mode"),
    ensureCaretAt(gPanel, 1),
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForSourceShown(gPanel, "-01.js")
  ]);

  backspaceText(gSearchBox, 4);

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    verifyContents([
      "code_script-switching-01.js?a=b",
      "code_test-editor-mode?c=d"
    ])
  ]));
}

function goDown() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    waitForSourceShown(gPanel, "test-editor-mode"),
  ]);

  EventUtils.sendKey("DOWN", gDebugger);

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "test-editor-mode"),
    ensureCaretAt(gPanel, 1),
    verifyContents([
      "code_script-switching-01.js?a=b",
      "code_test-editor-mode?c=d"
    ])
  ]));
}

function goDownAndWrap() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "test-editor-mode"),
    ensureCaretAt(gPanel, 1),
    waitForSourceShown(gPanel, "-01.js")
  ]);

  EventUtils.synthesizeKey("g", { metaKey: true }, gDebugger);

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    verifyContents([
      "code_script-switching-01.js?a=b",
      "code_test-editor-mode?c=d"
    ])
  ]));
}

function goUpAndWrap() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    waitForSourceShown(gPanel, "test-editor-mode")
  ]);

  EventUtils.synthesizeKey("G", { metaKey: true }, gDebugger);

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "test-editor-mode"),
    ensureCaretAt(gPanel, 1),
    verifyContents([
      "code_script-switching-01.js?a=b",
      "code_test-editor-mode?c=d"
    ])
  ]));
}

function goUp() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "test-editor-mode"),
    ensureCaretAt(gPanel, 1),
    waitForSourceShown(gPanel, "-01.js"),
  ]);

  EventUtils.sendKey("UP", gDebugger);

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    verifyContents([
      "code_script-switching-01.js?a=b",
      "code_test-editor-mode?c=d"
    ])
  ]));
}

function returnAndSwitch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    once(gDebugger, "popuphidden")
  ]);

  EventUtils.sendKey("RETURN", gDebugger);

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1)
  ]));
}

function clickAndSwitch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    once(gDebugger, "popuphidden"),
    waitForSourceShown(gPanel, "test-editor-mode")
  ]);

  EventUtils.sendMouseEvent({ type: "click" }, gSearchView.items[1].target, gDebugger);

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "test-editor-mode"),
    ensureCaretAt(gPanel, 1)
  ]));
}

function verifyContents(aMatches) {
  is(gSources.visibleItems.length, 3,
    "The unmatched sources in the widget should not be hidden.");
  is(gSearchView.itemCount, aMatches.length,
    "The filtered sources view should have the right items available.");

  for (let i = 0; i < gSearchView.itemCount; i++) {
    let trimmedLabel = gSourceUtils.trimUrlLength(gSourceUtils.trimUrlQuery(aMatches[i]));
    let trimmedLocation = gSourceUtils.trimUrlLength(EXAMPLE_URL + aMatches[i], 0, "start");

    ok(gSearchView.widget._parent.querySelector(".results-panel-item-label[value=\"" + trimmedLabel + "\"]"),
      "The filtered sources view should have the correct source labels.");
    ok(gSearchView.widget._parent.querySelector(".results-panel-item-label-below[value=\"" + trimmedLocation + "\"]"),
      "The filtered sources view should have the correct source locations.");
  }
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
  gSourceUtils = null;
  gSearchView = null;
  gSearchBox = null;
});
