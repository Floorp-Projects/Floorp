/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that inspecting an optimized out variable works when execution is
// paused.

"use strict";

function test() {
  Task.spawn(function* () {
    const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                     "test/test-closure-optimized-out.html";
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    let { toolbox, panel, panelWin } = yield openDebugger();

    let sources = panelWin.DebuggerView.Sources;
    yield panel.addBreakpoint({ actor: sources.values[0], line: 18 });
    yield ensureThreadClientState(panel, "resumed");

    let fetchedScopes = panelWin.once(panelWin.EVENTS.FETCHED_SCOPES);

    // Cause the debuggee to pause
    ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
      let button = content.document.querySelector("button");
      button.click();
    });

    yield fetchedScopes;
    ok(true, "Scopes were fetched");

    yield toolbox.selectTool("webconsole");

    // This is the meat of the test: evaluate the optimized out variable.
    hud.jsterm.execute("upvar");
    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "optimized out",
        category: CATEGORY_OUTPUT,
      }]
    });

    finishTest();
  }).then(null, aError => {
    ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
  });
}

// Debugger helper functions stolen from devtools/client/debugger/test/head.js.

function ensureThreadClientState(aPanel, aState) {
  let thread = aPanel.panelWin.gThreadClient;
  let state = thread.state;

  info("Thread is: '" + state + "'.");

  if (state == aState) {
    return promise.resolve(null);
  }
  return waitForThreadEvents(aPanel, aState);
}

function waitForThreadEvents(aPanel, aEventName, aEventRepeat = 1) {
  info("Waiting for thread event: '" + aEventName + "' to fire: " +
       aEventRepeat + " time(s).");

  let deferred = promise.defer();
  let thread = aPanel.panelWin.gThreadClient;
  let count = 0;

  thread.addListener(aEventName, function onEvent(eventName, ...args) {
    info("Thread event '" + eventName + "' fired: " + (++count) + " time(s).");

    if (count == aEventRepeat) {
      ok(true, "Enough '" + eventName + "' thread events have been fired.");
      thread.removeListener(eventName, onEvent);
      deferred.resolve.apply(deferred, args);
    }
  });

  return deferred.promise;
}
