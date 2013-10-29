/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests basic functionality of sources filtering (file search).
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gSources, gSearchView, gSearchBox;

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(3);

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gSearchView = gDebugger.DebuggerView.FilteredSources;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    waitForSourceShown(gPanel, "-01.js")
      .then(bogusSearch)
      .then(firstSearch)
      .then(secondSearch)
      .then(thirdSearch)
      .then(fourthSearch)
      .then(fifthSearch)
      .then(sixthSearch)
      .then(seventhSearch)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function bogusSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_NOT_FOUND)
  ]);

  setText(gSearchBox, "BOGUS");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    verifyContents({ itemCount: 0, hidden: true })
  ]));
}

function firstSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForSourceShown(gPanel, "-02.js")
  ]);

  setText(gSearchBox, "-02.js");

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-02.js"),
    ensureCaretAt(gPanel, 1),
    verifyContents({ itemCount: 1, hidden: false })
  ]));
}

function secondSearch() {
  let finished = promise.all([
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForSourceShown(gPanel, "-01.js")
  ])
  .then(() => {
    let finished = promise.all([
      once(gDebugger, "popupshown"),
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
      waitForCaretUpdated(gPanel, 5)
    ])
    .then(() => promise.all([
      ensureSourceIs(gPanel, "-01.js"),
      ensureCaretAt(gPanel, 5),
      verifyContents({ itemCount: 1, hidden: false })
    ]));

    typeText(gSearchBox, ":5");
    return finished;
  });

  setText(gSearchBox, ".*-01\.js");
  return finished;
}

function thirdSearch() {
  let finished = promise.all([
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForSourceShown(gPanel, "-02.js")
  ])
  .then(() => {
    let finished = promise.all([
      once(gDebugger, "popupshown"),
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
      waitForCaretUpdated(gPanel, 6, 12)
    ])
    .then(() => promise.all([
      ensureSourceIs(gPanel, "-02.js"),
      ensureCaretAt(gPanel, 6, 12),
      verifyContents({ itemCount: 1, hidden: false })
    ]));

    typeText(gSearchBox, "#deb");
    return finished;
  });

  setText(gSearchBox, ".*-02\.js");
  return finished;
}

function fourthSearch() {
  let finished = promise.all([
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForSourceShown(gPanel, "-01.js")
  ])
  .then(() => {
    let finished = promise.all([
      once(gDebugger, "popupshown"),
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
      waitForCaretUpdated(gPanel, 2, 9),
    ])
    .then(() => promise.all([
      ensureSourceIs(gPanel, "-01.js"),
      ensureCaretAt(gPanel, 2, 9),
      verifyContents({ itemCount: 1, hidden: false })
      // ...because we simply searched for ":" in the current file.
    ]));

    typeText(gSearchBox, "#:"); // # has precedence.
    return finished;
  });

  setText(gSearchBox, ".*-01\.js");
  return finished;
}

function fifthSearch() {
  let finished = promise.all([
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForSourceShown(gPanel, "-02.js")
  ])
  .then(() => {
    let finished = promise.all([
      once(gDebugger, "popuphidden"),
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_NOT_FOUND),
      waitForCaretUpdated(gPanel, 1, 3)
    ])
    .then(() => promise.all([
      ensureSourceIs(gPanel, "-02.js"),
      ensureCaretAt(gPanel, 1, 3),
      verifyContents({ itemCount: 0, hidden: true })
      // ...because the searched label includes ":5", so nothing is found.
    ]));

    typeText(gSearchBox, ":5#*"); // # has precedence.
    return finished;
  });

  setText(gSearchBox, ".*-02\.js");
  return finished;
}

function sixthSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-02.js"),
    ensureCaretAt(gPanel, 1, 3),
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForCaretUpdated(gPanel, 5)
  ]);

  backspaceText(gSearchBox, 2);

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-02.js"),
    ensureCaretAt(gPanel, 5),
    verifyContents({ itemCount: 1, hidden: false })
  ]));
}

function seventhSearch() {
  let finished = promise.all([
    ensureSourceIs(gPanel, "-02.js"),
    ensureCaretAt(gPanel, 5),
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND),
    waitForSourceShown(gPanel, "-01.js"),
  ]);

  backspaceText(gSearchBox, 6);

  return finished.then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1),
    verifyContents({ itemCount: 2, hidden: false })
  ]));
}

function verifyContents(aArgs) {
  is(gSources.visibleItems.length, 2,
    "The unmatched sources in the widget should not be hidden.");
  is(gSearchView.itemCount, aArgs.itemCount,
    "No sources should be displayed in the sources container after a bogus search.");
  is(gSearchView.hidden, aArgs.hidden,
    "No sources should be displayed in the sources container after a bogus search.");
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
  gSearchView = null;
  gSearchBox = null;
});
