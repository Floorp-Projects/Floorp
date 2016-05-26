/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that checking/unchecking an event listener's group in the view will
 * cause the active thread to get updated with the new event breakpoints for
 * all children inside that group.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-02.html";

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    let gDebugger = aPanel.panelWin;
    let gView = gDebugger.DebuggerView;
    let gController = gDebugger.DebuggerController;
    let gEvents = gView.EventListeners;
    let constants = gDebugger.require("./content/constants");

    Task.spawn(function* () {
      let fetched = waitForDispatch(aPanel, constants.FETCH_EVENT_LISTENERS);
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

      let updated = waitForDispatch(aPanel, constants.UPDATE_EVENT_BREAKPOINTS);
      EventUtils.sendMouseEvent({ type: "click" }, getGroupCheckboxNode("interactionEvents"), gDebugger);
      yield updated;

      testEventItem(0, true);
      testEventItem(1, false);
      testEventItem(2, false);
      testEventItem(3, false);
      testEventGroup("interactionEvents", true);
      testEventGroup("keyboardEvents", false);
      testEventGroup("mouseEvents", false);
      testEventArrays("change,click,keydown,keyup", "change");

      updated = waitForDispatch(aPanel, constants.UPDATE_EVENT_BREAKPOINTS);
      EventUtils.sendMouseEvent({ type: "click" }, getGroupCheckboxNode("interactionEvents"), gDebugger);
      yield updated;

      testEventItem(0, false);
      testEventItem(1, false);
      testEventItem(2, false);
      testEventItem(3, false);
      testEventGroup("interactionEvents", false);
      testEventGroup("keyboardEvents", false);
      testEventGroup("mouseEvents", false);
      testEventArrays("change,click,keydown,keyup", "");

      updated = waitForDispatch(aPanel, constants.UPDATE_EVENT_BREAKPOINTS);
      EventUtils.sendMouseEvent({ type: "click" }, getGroupCheckboxNode("keyboardEvents"), gDebugger);
      yield updated;

      testEventItem(0, false);
      testEventItem(1, false);
      testEventItem(2, true);
      testEventItem(3, true);
      testEventGroup("interactionEvents", false);
      testEventGroup("keyboardEvents", true);
      testEventGroup("mouseEvents", false);
      testEventArrays("change,click,keydown,keyup", "keydown,keyup");

      updated = waitForDispatch(aPanel, constants.UPDATE_EVENT_BREAKPOINTS);
      EventUtils.sendMouseEvent({ type: "click" }, getGroupCheckboxNode("keyboardEvents"), gDebugger);
      yield updated;

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
      is(gController.getState().eventListeners.activeEventNames.toString(), checked,
        "The correct event names are listed as being active breakpoints.");
    }
  });
}
