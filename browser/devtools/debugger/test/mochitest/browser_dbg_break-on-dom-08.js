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
    let gStore = gDebugger.store;
    let getState = gStore.getState;
    let constants = gDebugger.require('./content/constants');

    Task.spawn(function*() {
      yield waitForSourceShown(aPanel, ".html");
      yield callInTab(gTab, "addBodyClickEventListener");

      let fetched = afterDispatch(gStore, constants.FETCH_EVENT_LISTENERS);
      gView.toggleInstrumentsPane({ visible: true, animated: false }, 1);
      yield fetched;
      yield ensureThreadClientState(aPanel, "attached");

      is(gView.instrumentsPaneHidden, false,
        "The instruments pane should be visible.");
      is(gView.instrumentsPaneTab, "events-tab",
        "The events tab should be selected.");

      let updated = afterDispatch(gStore, constants.UPDATE_EVENT_BREAKPOINTS);
      EventUtils.sendMouseEvent({ type: "click" }, getItemCheckboxNode(1), gDebugger);
      yield updated;
      yield ensureThreadClientState(aPanel, "attached");

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
