/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that breaking on an event selects the variables view tab.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-02.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    let gTab = aTab;
    let gDebugger = aPanel.panelWin;
    let gView = gDebugger.DebuggerView;
    let gEvents = gView.EventListeners;

    Task.spawn(function*() {
      yield waitForSourceShown(aPanel, ".html");
      yield callInTab(gTab, "addBodyClickEventListener");

      let fetched = waitForDebuggerEvents(aPanel, gDebugger.EVENTS.EVENT_LISTENERS_FETCHED);
      gView.toggleInstrumentsPane({ visible: true, animated: false }, 1);
      yield fetched;
      yield ensureThreadClientState(aPanel, "resumed");

      is(gView.instrumentsPaneHidden, false,
        "The instruments pane should be visible.");
      is(gView.instrumentsPaneTab, "events-tab",
        "The events tab should be selected.");

      let updated = waitForDebuggerEvents(aPanel, gDebugger.EVENTS.EVENT_BREAKPOINTS_UPDATED);
      EventUtils.sendMouseEvent({ type: "click" }, getItemCheckboxNode(1), gDebugger);
      yield updated;
      yield ensureThreadClientState(aPanel, "resumed");

      let paused = waitForCaretAndScopes(aPanel, 48);
      generateMouseClickInTab(gTab, "content.document.body");
      yield paused;
      yield ensureThreadClientState(aPanel, "paused");

      is(gView.instrumentsPaneHidden, false,
        "The instruments pane should be visible.");
      is(gView.instrumentsPaneTab, "variables-tab",
        "The variables tab should be selected.");

      yield resumeDebuggerThenCloseAndFinish(aPanel);
    });

    function getItemCheckboxNode(index) {
      return gEvents.items[index].target.parentNode
        .querySelector(".side-menu-widget-item-checkbox");
    }
  });
}
