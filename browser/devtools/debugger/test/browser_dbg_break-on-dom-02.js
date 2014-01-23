/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that event listeners are fetched when the events tab is selected
 * or while sources are fetched and the events tab is focused.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-02.html";

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    let gDebugger = aPanel.panelWin;
    let gView = gDebugger.DebuggerView;
    let gEvents = gView.EventListeners;

    Task.spawn(function() {
      yield waitForSourceShown(aPanel, ".html");
      yield testFetchOnFocus();
      yield testFetchOnReloadWhenFocused();
      yield testFetchOnReloadWhenNotFocused();
      yield closeDebuggerAndFinish(aPanel);
    });

    function testFetchOnFocus() {
      return Task.spawn(function() {
        let fetched = waitForDebuggerEvents(aPanel, gDebugger.EVENTS.EVENT_LISTENERS_FETCHED);

        gView.toggleInstrumentsPane({ visible: true, animated: false }, 1);
        is(gView.instrumentsPaneHidden, false,
          "The instruments pane should be visible now.");
        is(gView.instrumentsPaneTab, "events-tab",
          "The events tab should be selected.");

        yield fetched;

        ok(true,
          "Event listeners were fetched when the events tab was selected");
        is(gEvents.itemCount, 4,
          "There should be 4 events displayed in the view.");
      });
    }

    function testFetchOnReloadWhenFocused() {
      return Task.spawn(function() {
        let fetched = waitForDebuggerEvents(aPanel, gDebugger.EVENTS.EVENT_LISTENERS_FETCHED);

        let reloading = once(gDebugger.gTarget, "will-navigate");
        let reloaded = waitForSourcesAfterReload();
        gDebugger.DebuggerController._target.activeTab.reload();

        yield reloading;

        is(gEvents.itemCount, 0,
          "There should be no events displayed in the view while reloading.");
        ok(true,
          "Event listeners were removed when the target started navigating.");

        yield reloaded;

        is(gView.instrumentsPaneHidden, false,
          "The instruments pane should still be visible.");
        is(gView.instrumentsPaneTab, "events-tab",
          "The events tab should still be selected.");

        yield fetched;

        is(gEvents.itemCount, 4,
          "There should be 4 events displayed in the view after reloading.");
        ok(true,
          "Event listeners were added back after the target finished navigating.");
      });
    }

    function testFetchOnReloadWhenNotFocused() {
      return Task.spawn(function() {
        gDebugger.on(gDebugger.EVENTS.EVENT_LISTENERS_FETCHED, () => {
          ok(false, "Shouldn't have fetched any event listeners.");
        });
        gDebugger.on(gDebugger.EVENTS.EVENT_BREAKPOINTS_UPDATED, () => {
          ok(false, "Shouldn't have updated any event breakpoints.");
        });

        gView.toggleInstrumentsPane({ visible: true, animated: false }, 0);
        is(gView.instrumentsPaneHidden, false,
          "The instruments pane should still be visible.");
        is(gView.instrumentsPaneTab, "variables-tab",
          "The variables tab should be selected.");

        let reloading = once(gDebugger.gTarget, "will-navigate");
        let reloaded = waitForSourcesAfterReload();
        gDebugger.DebuggerController._target.activeTab.reload();

        yield reloading;

        is(gEvents.itemCount, 0,
          "There should be no events displayed in the view while reloading.");
        ok(true,
          "Event listeners were removed when the target started navigating.");

        yield reloaded;

        is(gView.instrumentsPaneHidden, false,
          "The instruments pane should still be visible.");
        is(gView.instrumentsPaneTab, "variables-tab",
          "The variables tab should still be selected.");

        // Just to be really sure that the events will never ever fire.
        yield waitForTime(1000);

        is(gEvents.itemCount, 0,
          "There should be no events displayed in the view after reloading.");
        ok(true,
          "Event listeners were not added after the target finished navigating.");
      });
    }

    function waitForSourcesAfterReload() {
      return promise.all([
        waitForDebuggerEvents(aPanel, gDebugger.EVENTS.NEW_SOURCE),
        waitForDebuggerEvents(aPanel, gDebugger.EVENTS.SOURCES_ADDED),
        waitForDebuggerEvents(aPanel, gDebugger.EVENTS.SOURCE_SHOWN)
      ]);
    }
  });
}
