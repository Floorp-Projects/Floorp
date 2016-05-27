/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that system event listeners don't get duplicated in the view.
 */

function test() {
  initDebugger().then(([aTab,, aPanel]) => {
    let gDebugger = aPanel.panelWin;
    let gView = gDebugger.DebuggerView;
    let gEvents = gView.EventListeners;
    let gL10N = gDebugger.L10N;

    is(gEvents.itemCount, 0,
      "There are no events displayed in the corresponding pane yet.");

    gEvents.addListener({
      type: "foo",
      node: { selector: "#first" },
      function: { url: null }
    });

    is(gEvents.itemCount, 1,
      "There was a system event listener added in the view.");
    is(gEvents.attachments[0].url, gL10N.getStr("eventNative"),
      "The correct string is used as the event's url.");
    is(gEvents.attachments[0].type, "foo",
      "The correct string is used as the event's type.");
    is(gEvents.attachments[0].selectors.toString(), "#first",
      "The correct array of selectors is used as the event's target.");

    gEvents.addListener({
      type: "bar",
      node: { selector: "#second" },
      function: { url: null }
    });

    is(gEvents.itemCount, 2,
      "There was another system event listener added in the view.");
    is(gEvents.attachments[1].url, gL10N.getStr("eventNative"),
      "The correct string is used as the event's url.");
    is(gEvents.attachments[1].type, "bar",
      "The correct string is used as the event's type.");
    is(gEvents.attachments[1].selectors.toString(), "#second",
      "The correct array of selectors is used as the event's target.");

    gEvents.addListener({
      type: "foo",
      node: { selector: "#first" },
      function: { url: null }
    });

    is(gEvents.itemCount, 2,
      "There wasn't another system event listener added in the view.");
    is(gEvents.attachments[0].url, gL10N.getStr("eventNative"),
      "The correct string is used as the event's url.");
    is(gEvents.attachments[0].type, "foo",
      "The correct string is used as the event's type.");
    is(gEvents.attachments[0].selectors.toString(), "#first",
      "The correct array of selectors is used as the event's target.");

    gEvents.addListener({
      type: "foo",
      node: { selector: "#second" },
      function: { url: null }
    });

    is(gEvents.itemCount, 2,
      "There still wasn't another system event listener added in the view.");
    is(gEvents.attachments[0].url, gL10N.getStr("eventNative"),
      "The correct string is used as the event's url.");
    is(gEvents.attachments[0].type, "foo",
      "The correct string is used as the event's type.");
    is(gEvents.attachments[0].selectors.toString(), "#first,#second",
      "The correct array of selectors is used as the event's target.");


    gEvents.addListener({
      type: null,
      node: { selector: "#bogus" },
      function: { url: null }
    });

    is(gEvents.itemCount, 2,
      "No bogus system event listener was added in the view.");

    is(gEvents.attachments[0].url, gL10N.getStr("eventNative"),
      "The correct string is used as the first event's url.");
    is(gEvents.attachments[0].type, "foo",
      "The correct string is used as the first event's type.");
    is(gEvents.attachments[0].selectors.toString(), "#first,#second",
      "The correct array of selectors is used as the first event's target.");

    is(gEvents.attachments[1].url, gL10N.getStr("eventNative"),
      "The correct string is used as the second event's url.");
    is(gEvents.attachments[1].type, "bar",
      "The correct string is used as the second event's type.");
    is(gEvents.attachments[1].selectors.toString(), "#second",
      "The correct array of selectors is used as the second event's target.");

    closeDebuggerAndFinish(aPanel);
  });
}
