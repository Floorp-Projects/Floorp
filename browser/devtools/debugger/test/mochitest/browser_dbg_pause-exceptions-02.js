/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that pausing on exceptions works after reload.
 */

const TAB_URL = EXAMPLE_URL + "doc_pause-exceptions.html";

var gTab, gPanel, gDebugger;
var gFrames, gVariables, gPrefs, gOptions;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gVariables = gDebugger.DebuggerView.Variables;
    gPrefs = gDebugger.Prefs;
    gOptions = gDebugger.DebuggerView.Options;

    is(gPrefs.pauseOnExceptions, false,
      "The pause-on-exceptions pref should be disabled by default.");
    isnot(gOptions._pauseOnExceptionsItem.getAttribute("checked"), "true",
      "The pause-on-exceptions menu item should not be checked.");

    enablePauseOnExceptions()
      .then(disableIgnoreCaughtExceptions)
      .then(() => reloadActiveTab(gPanel, gDebugger.EVENTS.SOURCE_SHOWN))
      .then(testPauseOnExceptionsAfterReload)
      .then(disablePauseOnExceptions)
      .then(enableIgnoreCaughtExceptions)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testPauseOnExceptionsAfterReload() {
  let finished = waitForCaretAndScopes(gPanel, 19).then(() => {
    info("Testing enabled pause-on-exceptions.");

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
      info("Testing enabled pause-on-exceptions and resumed after pause.");

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

function enablePauseOnExceptions() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(gPrefs.pauseOnExceptions, true,
      "The pause-on-exceptions pref should now be enabled.");
    is(gOptions._pauseOnExceptionsItem.getAttribute("checked"), "true",
      "The pause-on-exceptions menu item should now be checked.");

    ok(true, "Pausing on exceptions was enabled.");
    deferred.resolve();
  });

  gOptions._pauseOnExceptionsItem.setAttribute("checked", "true");
  gOptions._togglePauseOnExceptions();

  return deferred.promise;
}

function disablePauseOnExceptions() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(gPrefs.pauseOnExceptions, false,
      "The pause-on-exceptions pref should now be disabled.");
    isnot(gOptions._pauseOnExceptionsItem.getAttribute("checked"), "true",
      "The pause-on-exceptions menu item should now be unchecked.");

    ok(true, "Pausing on exceptions was disabled.");
    deferred.resolve();
  });

  gOptions._pauseOnExceptionsItem.setAttribute("checked", "false");
  gOptions._togglePauseOnExceptions();

  return deferred.promise;
}

function enableIgnoreCaughtExceptions() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(gPrefs.ignoreCaughtExceptions, true,
      "The ignore-caught-exceptions pref should now be enabled.");
    is(gOptions._ignoreCaughtExceptionsItem.getAttribute("checked"), "true",
      "The ignore-caught-exceptions menu item should now be checked.");

    ok(true, "Ignore caught exceptions was enabled.");
    deferred.resolve();
  });

  gOptions._ignoreCaughtExceptionsItem.setAttribute("checked", "true");
  gOptions._toggleIgnoreCaughtExceptions();

  return deferred.promise;
}

function disableIgnoreCaughtExceptions() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(gPrefs.ignoreCaughtExceptions, false,
      "The ignore-caught-exceptions pref should now be disabled.");
    isnot(gOptions._ignoreCaughtExceptionsItem.getAttribute("checked"), "true",
      "The ignore-caught-exceptions menu item should now be unchecked.");

    ok(true, "Ignore caught exceptions was disabled.");
    deferred.resolve();
  });

  gOptions._ignoreCaughtExceptionsItem.setAttribute("checked", "false");
  gOptions._toggleIgnoreCaughtExceptions();

  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gFrames = null;
  gVariables = null;
  gPrefs = null;
  gOptions = null;
});
