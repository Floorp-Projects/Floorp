/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the event listener states are preserved in the view after the
 * target navigates.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-02.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    let gDebugger = aPanel.panelWin;
    let gView = gDebugger.DebuggerView;
    let gController = gDebugger.DebuggerController
    let gEvents = gView.EventListeners;
    let gBreakpoints = gController.Breakpoints;
    let gStore = gDebugger.store;
    let getState = gStore.getState;
    let constants = gDebugger.require('./content/constants');

    Task.spawn(function*() {
      yield waitForSourceShown(aPanel, ".html");

      let fetched = afterDispatch(gStore, constants.FETCH_EVENT_LISTENERS);
      gView.toggleInstrumentsPane({ visible: true, animated: false }, 1);
      yield fetched;

      testEventItem(0, false);
      testEventItem(1, false);
      testEventItem(2, false);
      testEventItem(3, false);
      testEventGroup("interactionEvents", false);
      testEventGroup("keyboardEvents", false);
      testEventGroup("mouseEvents", false);
      testEventArrays("change,click,keydown,keyup", "");

      let updated = afterDispatch(gStore, constants.UPDATE_EVENT_BREAKPOINTS);
      EventUtils.sendMouseEvent({ type: "click" }, getItemCheckboxNode(0), gDebugger);
      EventUtils.sendMouseEvent({ type: "click" }, getItemCheckboxNode(1), gDebugger);
      EventUtils.sendMouseEvent({ type: "click" }, getItemCheckboxNode(2), gDebugger);
      yield updated;

      testEventItem(0, true);
      testEventItem(1, true);
      testEventItem(2, true);
      testEventItem(3, false);
      testEventGroup("interactionEvents", false);
      testEventGroup("keyboardEvents", false);
      testEventGroup("mouseEvents", false);
      testEventArrays("change,click,keydown,keyup", "change,click,keydown");

      reload(aPanel);
      yield afterDispatch(gStore, constants.FETCH_EVENT_LISTENERS);

      testEventItem(0, true);
      testEventItem(1, true);
      testEventItem(2, true);
      testEventItem(3, false);
      testEventGroup("interactionEvents", false);
      testEventGroup("keyboardEvents", false);
      testEventGroup("mouseEvents", false);
      testEventArrays("change,click,keydown,keyup", "change,click,keydown");

      updated = afterDispatch(gStore, constants.UPDATE_EVENT_BREAKPOINTS);
      EventUtils.sendMouseEvent({ type: "click" }, getItemCheckboxNode(0), gDebugger);
      EventUtils.sendMouseEvent({ type: "click" }, getItemCheckboxNode(1), gDebugger);
      EventUtils.sendMouseEvent({ type: "click" }, getItemCheckboxNode(2), gDebugger);
      yield updated;

      testEventItem(0, false);
      testEventItem(1, false);
      testEventItem(2, false);
      testEventItem(3, false);
      testEventGroup("interactionEvents", false);
      testEventGroup("keyboardEvents", false);
      testEventGroup("mouseEvents", false);
      testEventArrays("change,click,keydown,keyup", "");

      reload(aPanel);
      yield afterDispatch(gStore, constants.FETCH_EVENT_LISTENERS);

      testEventItem(0, false);
      testEventItem(1, false);
      testEventItem(2, false);
      testEventItem(3, false);
      testEventGroup("interactionEvents", false);
      testEventGroup("keyboardEvents", false);
      testEventGroup("mouseEvents", false);
      testEventArrays("change,click,keydown,keyup", "");

      yield ensureThreadClientState(aPanel, "attached");
      yield closeDebuggerAndFinish(aPanel);
    });

    function getItemCheckboxNode(index) {
      return gEvents.items[index].target.parentNode
        .querySelector(".side-menu-widget-item-checkbox");
    }

    function getGroupCheckboxNode(string) {
      return gEvents.widget._parent
        .querySelector(".side-menu-widget-group[name=" + gDebugger.L10N.getStr(string) + "]")
        .querySelector(".side-menu-widget-group-checkbox");
    }

    function testEventItem(index, checked) {
      is(gEvents.attachments[index].checkboxState, checked,
        "The event at index " + index + " has the correct checkbox state.");
      is(getItemCheckboxNode(index).checked, checked,
        "The correct checkbox state is shown for this event.");
    }

    function testEventGroup(string, checked) {
      is(getGroupCheckboxNode(string).checked, checked,
        "The correct checkbox state is shown for the group " + string + ".");
    }

    function testEventArrays(all, checked) {
      is(gEvents.getAllEvents().toString(), all,
        "The getAllEvents() method returns the correct stuff.");
      is(gEvents.getCheckedEvents().toString(), checked,
         "The getCheckedEvents() method returns the correct stuff.");
      is(getState().eventListeners.activeEventNames.toString(), checked,
         "The correct event names are listed as being active breakpoints.");
    }
  });
}
