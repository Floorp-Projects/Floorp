 /* Any copyright is dedicated to the Public Domain.
    http://creativecommons.org/publicdomain/zero/1.0/ */

const scratchpads = [
  { text: "text1", executionContext: 1 },
  { text: "", executionContext: 2, filename: "test.js" }
];

const testState = {
  windows: [{
    tabs: [
      { entries: [{ url: "about:blank" }] },
    ]
  }],
  global: {
    scratchpads: JSON.stringify(scratchpads)
  }
};

// only finish() when correct number of windows opened
var restored = [];
function addState(state) {
  restored.push(state);

  if (restored.length == scratchpads.length) {
    ok(statesMatch(restored, scratchpads),
      "Two scratchpad windows restored");

    Services.ww.unregisterNotification(windowObserver);
    finish();
  }
}

function test() {
  waitForExplicitFinish();

  Services.ww.registerNotification(windowObserver);

  ss.setBrowserState(JSON.stringify(testState));
}

function windowObserver(aSubject, aTopic, aData) {
  if (aTopic == "domwindowopened") {
    let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);

      if (win.Scratchpad) {
        win.Scratchpad.addObserver({
          onReady: function() {
            win.Scratchpad.removeObserver(this);

            let state = win.Scratchpad.getState();
            win.close();
            addState(state);
          },
        });
      }
    }, false);
  }
}

function statesMatch(restored, states) {
  return states.every(function(state) {
    return restored.some(function(restoredState) {
      return state.filename == restoredState.filename &&
             state.text == restoredState.text &&
             state.executionContext == restoredState.executionContext;
    })
  });
}
