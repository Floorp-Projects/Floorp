/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test to make sure that web console commands can fire while paused at a
// breakpoint that was triggered from a JS call.  Relies on asynchronous js
// evaluation over the protocol - see Bug 1088861.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-eval-in-stackframe.html";
var test = asyncTest(function*() {
  yield loadTab(TEST_URI);

  info("open the web console");
  let hud = yield openConsole();
  let {jsterm} = hud;

  info("open the debugger");
  let {panelWin} = yield openDebugger();
  let {DebuggerController} = panelWin;
  let {activeThread} = DebuggerController;

  let firstCall = promise.defer();
  let frameAdded = promise.defer();
  executeSoon(() => {
    info("Executing firstCall");
    activeThread.addOneTimeListener("framesadded", () => {
      executeSoon(frameAdded.resolve);
    });
    jsterm.execute("firstCall()").then(firstCall.resolve);
  });

  info("Waiting for a frame to be added");
  yield frameAdded.promise;

  info("Executing basic command while paused");
  yield executeAndConfirm(jsterm, "1 + 2", "3");

  info("Executing command using scoped variables while paused");
  yield executeAndConfirm(jsterm, "foo + foo2",
                          '"globalFooBug783499foo2SecondCall"');

  info("Resuming the thread");
  activeThread.resume();

  info("Checking the first command, which is the last to resolve since it " +
       "paused");
  let node = yield firstCall.promise;
  is(node.querySelector(".message-body").textContent,
     "undefined",
     "firstCall() returned correct value");
});

function* executeAndConfirm(jsterm, input, output) {
  info("Executing command `" + input + "`");

  let node = yield jsterm.execute(input);

  is(node.querySelector(".message-body").textContent, output,
     "Expected result from call to " + input);
}
