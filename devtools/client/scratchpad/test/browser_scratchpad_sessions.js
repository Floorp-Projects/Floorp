 /* Any copyright is dedicated to the Public Domain.
    http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {Utils} = ChromeUtils.import("resource://gre/modules/sessionstore/Utils.jsm", {});
const triggeringPrincipal_base64 = Utils.SERIALIZED_SYSTEMPRINCIPAL;

const testState = {
  windows: [{
    tabs: [
      { entries: [{ url: "about:blank", triggeringPrincipal_base64 }] },
    ]
  }],
  scratchpads: [
    { text: "text1", executionContext: 1 },
    { text: "", executionContext: 2, filename: "test.js" }
  ]
};

// only finish() when correct number of windows opened
var restoredStates = [];
function addState(state) {
  restoredStates.push(state);

  if (restoredStates.length == testState.scratchpads.length) {
    ok(statesMatch(restoredStates, testState.scratchpads),
      "Two scratchpad windows restored");

    Services.ww.unregisterNotification(windowObserver);
    finish();
  }
}

function test() {
  waitForExplicitFinish();

  Services.ww.registerNotification(windowObserver);

  SessionStore.setBrowserState(JSON.stringify(testState));
}

function windowObserver(subject, topic, data) {
  if (topic == "domwindowopened") {
    const win = subject.QueryInterface(Ci.nsIDOMWindow);
    win.addEventListener("load", function() {
      if (win.Scratchpad) {
        win.Scratchpad.addObserver({
          onReady() {
            win.Scratchpad.removeObserver(this);

            const state = win.Scratchpad.getState();
            BrowserTestUtils.closeWindow(win).then(() => {
              addState(state);
            });
          },
        });
      }
    }, {once: true});
  }
}

function statesMatch(restored, states) {
  return states.every(function(state) {
    return restored.some(function(restoredState) {
      return state.filename == restoredState.filename &&
             state.text == restoredState.text &&
             state.executionContext == restoredState.executionContext;
    });
  });
}
