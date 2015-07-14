/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that pausing on exceptions works.
 */

const TAB_URL = EXAMPLE_URL + "doc_pause-exceptions.html";

let gTab, gPanel, gDebugger;
let gFrames, gVariables, gPrefs;

function test() {
  requestLongerTimeout(2);
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gVariables = gDebugger.DebuggerView.Variables;
    gPrefs = gDebugger.Prefs;

    is(gPrefs.pauseOnExceptions, false,
      "The pause-on-exceptions pref should be disabled by default.");

    testPauseOnExceptionsDisabled()
      .then(clickToPauseOnAllExceptions)
      .then(testPauseOnAllExceptionsEnabled)
      .then(clickToPauseOnUncaughtExceptions)
      .then(testPauseOnUncaughtExceptionsEnabled)
      .then(clickToStopPauseOnExceptions)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testPauseOnExceptionsDisabled() {
  let finished = waitForCaretAndScopes(gPanel, 26).then(() => {
    info("Testing disabled pause-on-exceptions.");

    is(gDebugger.gThreadClient.state, "paused",
      "Should only be getting stack frames while paused (1).");
    ok(isCaretPos(gPanel, 26),
      "Should be paused on the debugger statement (1).");

    let innerScope = gVariables.getScopeAtIndex(0);
    let innerNodes = innerScope.target.querySelector(".variables-view-element-details").childNodes;

    is(gFrames.itemCount, 1,
      "Should have one frame.");
    is(gVariables._store.length, 3,
      "Should have three scopes.");

    is(innerNodes[0].querySelector(".name").getAttribute("value"), "this",
      "Should have the right property name for 'this'.");
    is(innerNodes[0].querySelector(".value").getAttribute("value"), "<button>",
      "Should have the right property value for 'this'.");

    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_CLEARED).then(() => {
      isnot(gDebugger.gThreadClient.state, "paused",
        "Should not be paused after resuming.");
      ok(isCaretPos(gPanel, 26),
        "Should be idle on the debugger statement.");

      ok(true, "Frames were cleared, debugger didn't pause again.");
    });

    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);

    return finished;
  });

  generateMouseClickInTab(gTab, "content.document.querySelector('button')");

  return finished;
}

function testPauseOnUncaughtExceptionsEnabled() {
  let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES).then(() => {
    info("Testing enabled pause-on-uncaught-exceptions only.");

    is(gDebugger.gThreadClient.state, "paused",
      "Should only be getting stack frames while paused (1).");
    ok(isCaretPos(gPanel, 26),
      "Should be paused on the debugger statement (1).");

    let innerScope = gVariables.getScopeAtIndex(0);
    let innerNodes = innerScope.target.querySelector(".variables-view-element-details").childNodes;

    is(gFrames.itemCount, 1,
      "Should have one frame.");
    is(gVariables._store.length, 3,
      "Should have three scopes.");

    is(innerNodes[0].querySelector(".name").getAttribute("value"), "this",
      "Should have the right property name for 'this'.");
    is(innerNodes[0].querySelector(".value").getAttribute("value"), "<button>",
      "Should have the right property value for 'this'.");

    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_CLEARED).then(() => {
      isnot(gDebugger.gThreadClient.state, "paused",
        "Should not be paused after resuming.");
      ok(isCaretPos(gPanel, 26),
        "Should be idle on the debugger statement.");

      ok(true, "Frames were cleared, debugger didn't pause again.");
    });

    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);

    return finished;
  });

  generateMouseClickInTab(gTab, "content.document.querySelector('button')");

  return finished;
}

function testPauseOnAllExceptionsEnabled() {
  let finished = waitForCaretAndScopes(gPanel, 19).then(() => {
    info("Testing enabled pause-on-all-exceptions.");

    is(gDebugger.gThreadClient.state, "paused",
      "Should only be getting stack frames while paused.");
    ok(isCaretPos(gPanel, 19),
      "Should be paused on the debugger statement.");

    let innerScope = gVariables.getScopeAtIndex(0);
    let innerNodes = innerScope.target.querySelector(".variables-view-element-details").childNodes;

    is(gFrames.itemCount, 1,
      "Should have one frame.");
    is(gVariables._store.length, 3,
      "Should have three scopes.");

    is(innerNodes[0].querySelector(".name").getAttribute("value"), "<exception>",
      "Should have the right property name for <exception>.");
    is(innerNodes[0].querySelector(".value").getAttribute("value"), "Error",
      "Should have the right property value for <exception>.");

    let finished = waitForCaretAndScopes(gPanel, 26).then(() => {
      info("Testing enabled pause-on-all-exceptions and resumed after pause.");

      is(gDebugger.gThreadClient.state, "paused",
        "Should only be getting stack frames while paused.");
      ok(isCaretPos(gPanel, 26),
        "Should be paused on the debugger statement.");

      let innerScope = gVariables.getScopeAtIndex(0);
      let innerNodes = innerScope.target.querySelector(".variables-view-element-details").childNodes;

      is(gFrames.itemCount, 1,
        "Should have one frame.");
      is(gVariables._store.length, 3,
        "Should have three scopes.");

      is(innerNodes[0].querySelector(".name").getAttribute("value"), "this",
        "Should have the right property name for 'this'.");
      is(innerNodes[0].querySelector(".value").getAttribute("value"), "<button>",
        "Should have the right property value for 'this'.");

      let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_CLEARED).then(() => {
        isnot(gDebugger.gThreadClient.state, "paused",
          "Should not be paused after resuming.");
        ok(isCaretPos(gPanel, 26),
          "Should be idle on the debugger statement.");

        ok(true, "Frames were cleared, debugger didn't pause again.");
      });

      EventUtils.sendMouseEvent({ type: "mousedown" },
        gDebugger.document.getElementById("resume"),
        gDebugger);

      return finished;
    });

    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);

    return finished;
  });

  generateMouseClickInTab(gTab, "content.document.querySelector('button')");

  return finished;
}

function clickToPauseOnAllExceptions() {
  var deferred = promise.defer();
  var pauseOnExceptionsButton = getPauseOnExceptionsButton();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(pauseOnExceptionsButton.getAttribute("tooltiptext"),
      "Pause on uncaught exceptions",
      "The button's tooltip text should be 'Pause on uncaught exceptions'.");
    is(pauseOnExceptionsButton.getAttribute("state"), 1,
      "The pause on exceptions button state variable should be 1");

      deferred.resolve();
  });

  pauseOnExceptionsButton.click();

  return deferred.promise;
}

function clickToPauseOnUncaughtExceptions() {
  var deferred = promise.defer();
  var pauseOnExceptionsButton = getPauseOnExceptionsButton();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () =>{
    is(pauseOnExceptionsButton.getAttribute("tooltiptext"),
      "Do not pause on exceptions",
      "The button's tooltip text should be 'Do not pause on exceptions'.");
    is(pauseOnExceptionsButton.getAttribute("state"), 2,
      "The pause on exceptions button state variable should be 2");

      deferred.resolve();
  });
  pauseOnExceptionsButton.click();
  return deferred.promise;
}

function clickToStopPauseOnExceptions() {
  var deferred = promise.defer();
  var pauseOnExceptionsButton = getPauseOnExceptionsButton();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () =>{
    is(pauseOnExceptionsButton.getAttribute("tooltiptext"),
      "Pause on all exceptions",
      "The button's tooltip text should be 'Pause on all exceptions'.");
    is(pauseOnExceptionsButton.getAttribute("state"), 0,
      "The pause on exceptions button state variable should be 0");

      deferred.resolve();
  });
  pauseOnExceptionsButton.click();
  return deferred.promise;
}

function getPauseOnExceptionsButton() {
  return gDebugger.document.getElementById("toggle-pause-exceptions");
}


registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gFrames = null;
  gVariables = null;
  gPrefs = null;
});
