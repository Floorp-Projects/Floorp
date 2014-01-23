/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that event listeners aren't fetched when the events tab isn't selected.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-02.html";

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    let gDebugger = aPanel.panelWin;
    let gView = gDebugger.DebuggerView;
    let gEvents = gView.EventListeners;

    gDebugger.on(gDebugger.EVENTS.EVENT_LISTENERS_FETCHED, () => {
      ok(false, "Shouldn't have fetched any event listeners.");
    });
    gDebugger.on(gDebugger.EVENTS.EVENT_BREAKPOINTS_UPDATED, () => {
      ok(false, "Shouldn't have updated any event breakpoints.");
    });

    gView.toggleInstrumentsPane({ visible: true, animated: false });

    is(gView.instrumentsPaneHidden, false,
      "The instruments pane should be visible now.");
    is(gView.instrumentsPaneTab, "variables-tab",
      "The variables tab should be selected by default.");

    Task.spawn(function() {
      yield waitForSourceShown(aPanel, ".html");
      is(gEvents.itemCount, 0, "There should be no events before reloading.");

      let reloaded = waitForSourcesAfterReload();
      gDebugger.DebuggerController._target.activeTab.reload();

      is(gEvents.itemCount, 0, "There should be no events while reloading.");
      yield reloaded;
      is(gEvents.itemCount, 0, "There should be no events after reloading.");

      yield closeDebuggerAndFinish(aPanel);
    });

    function waitForSourcesAfterReload() {
      return promise.all([
        waitForDebuggerEvents(aPanel, gDebugger.EVENTS.NEW_SOURCE),
        waitForDebuggerEvents(aPanel, gDebugger.EVENTS.SOURCES_ADDED),
        waitForDebuggerEvents(aPanel, gDebugger.EVENTS.SOURCE_SHOWN)
      ]);
    }
  });
}
