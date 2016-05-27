/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests basic functionality of sources filtering (file search).
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(3);

  let options = {
    source: "-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;
    const gSearchView = gDebugger.DebuggerView.Filtering.FilteredSources;
    const gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    Task.spawn(function* () {
      // move searches to yields
      // not sure what to do with the error...
      yield bogusSearch();
      yield firstSearch();
      yield secondSearch();
      yield thirdSearch();
      yield fourthSearch();
      yield fifthSearch();
      yield sixthSearch();
      yield seventhSearch();

      return closeDebuggerAndFinish(gPanel)
        .then(null, aError => {
          ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
        });
    });

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
          waitForCaretUpdated(gPanel, 6, 6)
        ])
        .then(() => promise.all([
          ensureSourceIs(gPanel, "-02.js"),
          ensureCaretAt(gPanel, 6, 6),
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
        ensureCaretAt(gPanel, 2, 9),
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

  });
}
