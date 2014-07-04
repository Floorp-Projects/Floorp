/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that checking/unchecking an event listener in the view correctly
 * causes the active thread to get updated with the new event breakpoints.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-02.html";

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    let gDebugger = aPanel.panelWin;
    let gView = gDebugger.DebuggerView;
    let gController = gDebugger.DebuggerController
    let gEvents = gView.EventListeners;
    let gBreakpoints = gController.Breakpoints;

    Task.spawn(function() {
      yield waitForSourceShown(aPanel, ".html");

      let fetched = waitForDebuggerEvents(aPanel, gDebugger.EVENTS.EVENT_LISTENERS_FETCHED);
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

      let updated = waitForDebuggerEvents(aPanel, gDebugger.EVENTS.EVENT_BREAKPOINTS_UPDATED);
      EventUtils.sendMouseEvent({ type: "click" }, getItemCheckboxNode(0), gDebugger);
      yield updated;

      testEventItem(0, true);
      testEventItem(1, false);
      testEventItem(2, false);
      testEventItem(3, false);
      testEventGroup("interactionEvents", false);
      testEventGroup("keyboardEvents", false);
      testEventGroup("mouseEvents", false);
      testEventArrays("change,click,keydown,keyup", "change");

      let updated = waitForDebuggerEvents(aPanel, gDebugger.EVENTS.EVENT_BREAKPOINTS_UPDATED);
      EventUtils.sendMouseEvent({ type: "click" }, getItemCheckboxNode(0), gDebugger);
      yield updated;

      testEventItem(0, false);
      testEventItem(1, false);
      testEventItem(2, false);
      testEventItem(3, false);
      testEventGroup("interactionEvents", false);
      testEventGroup("keyboardEvents", false);
      testEventGroup("mouseEvents", false);
      testEventArrays("change,click,keydown,keyup", "");

      yield ensureThreadClientState(aPanel, "resumed");
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
      is(gBreakpoints.DOM.activeEventNames.toString(), checked,
        "The correct event names are listed as being active breakpoints.");
    }
  });
}
