/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that event listeners are properly displayed in the view.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-02.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    let gDebugger = aPanel.panelWin;
    let gView = gDebugger.DebuggerView;
    let gEvents = gView.EventListeners;

    Task.spawn(function*() {
      yield waitForSourceShown(aPanel, ".html");

      let fetched = waitForDebuggerEvents(aPanel, gDebugger.EVENTS.EVENT_LISTENERS_FETCHED);
      gView.toggleInstrumentsPane({ visible: true, animated: false }, 1);
      yield fetched;

      is(gEvents.widget._parent.querySelectorAll(".side-menu-widget-group").length, 3,
        "There should be 3 groups shown in the view.");
      is(gEvents.widget._parent.querySelectorAll(".side-menu-widget-group-checkbox").length, 3,
        "There should be a checkbox for each group shown in the view.");

      is(gEvents.widget._parent.querySelectorAll(".side-menu-widget-item").length, 4,
        "There should be 4 items shown in the view.");
      is(gEvents.widget._parent.querySelectorAll(".side-menu-widget-item-checkbox").length, 4,
        "There should be a checkbox for each item shown in the view.");

      testEventItem(0, "doc_event-listeners-02.html", "change", ["body > input:nth-child(2)"], false);
      testEventItem(1, "doc_event-listeners-02.html", "click", ["body > button:nth-child(1)"], false);
      testEventItem(2, "doc_event-listeners-02.html", "keydown", ["window", "body"], false);
      testEventItem(3, "doc_event-listeners-02.html", "keyup", ["body > input:nth-child(2)"], false);

      testEventGroup("interactionEvents", false);
      testEventGroup("keyboardEvents", false);
      testEventGroup("mouseEvents", false);

      is(gEvents.getAllEvents().toString(), "change,click,keydown,keyup",
        "The getAllEvents() method returns the correct stuff.");
      is(gEvents.getCheckedEvents().toString(), "",
        "The getCheckedEvents() method returns the correct stuff.");

      yield ensureThreadClientState(aPanel, "resumed");
      yield closeDebuggerAndFinish(aPanel);
    });

    function testEventItem(index, label, type, selectors, checked) {
      let item = gEvents.items[index];
      let node = item.target;

      ok(item.attachment.url.includes(label),
        "The event at index " + index + " has the correct url.");
      is(item.attachment.type, type,
        "The event at index " + index + " has the correct type.");
      is(item.attachment.selectors.toString(), selectors,
        "The event at index " + index + " has the correct selectors.");
      is(item.attachment.checkboxState, checked,
        "The event at index " + index + " has the correct checkbox state.");

      let targets = selectors.length > 1
        ? gDebugger.L10N.getFormatStr("eventNodes", selectors.length)
        : selectors.toString();

      is(node.querySelector(".dbg-event-listener-type").getAttribute("value"), type,
        "The correct type is shown for this event.");
      is(node.querySelector(".dbg-event-listener-targets").getAttribute("value"), targets,
        "The correct target is shown for this event.");
      is(node.querySelector(".dbg-event-listener-location").getAttribute("value"), label,
        "The correct location is shown for this event.");
      is(node.parentNode.querySelector(".side-menu-widget-item-checkbox").checked, checked,
        "The correct checkbox state is shown for this event.");
    }

    function testEventGroup(string, checked) {
      let name = gDebugger.L10N.getStr(string);
      let group = gEvents.widget._parent
        .querySelector(".side-menu-widget-group[name=" + name + "]");

      is(group.querySelector(".side-menu-widget-group-title > .name").value, name,
        "The correct label is shown for the group named " + name + ".");
      is(group.querySelector(".side-menu-widget-group-checkbox").checked, checked,
        "The correct checkbox state is shown for the group named " + name + ".");
    }
  });
}
