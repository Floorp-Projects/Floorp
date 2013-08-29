/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let Task = Cu.import("resource://gre/modules/Task.jsm", {}).Task;

  return Task.spawn(function() {
    const TEST_URI = "http://example.com/browser/browser/devtools/debugger/" +
                     "test/browser_dbg_cmd.html";

    let options = yield helpers.openTab(TEST_URI);
    yield helpers.openToolbar(options);

    yield helpers.audit(options, [{
      setup: "dbg open",
      exec: { output: "", completed: false }
    }]);

    let toolbox = yield gDevTools.showToolbox(options.target, "jsdebugger");
    let dbg = toolbox.getCurrentPanel();
    ok(dbg, "DebuggerPanel exists");

    // Wait for the initial resume...
    let resumeDeferred = promise.defer();
    dbg.panelWin.gClient.addOneTimeListener("resumed", () => {
      resumeDeferred.resolve();
    });
    yield resumeDeferred.promise;

    yield helpers.audit(options, [{
      setup: "dbg list",
      exec: { output: /browser_dbg_cmd.html/ }
    }]);

    // exec a command with GCLI to resume the debugger and wait until it stops
    let cmd = function(typed) {
      let cmdDeferred = promise.defer();
      dbg._controller.activeThread.addOneTimeListener("paused", ev => {
        cmdDeferred.resolve();
      });
      helpers.audit(options, [{
        setup: typed,
        exec: { output: "" }
      }]).then(null, cmdDeferred.reject);
      return cmdDeferred.promise;
    };

    yield cmd("dbg interrupt");

    let interruptDeferred = promise.defer();
    ok(true, "debugger is paused");
    dbg._controller.activeThread.addOneTimeListener("resumed", () => {
      ok(true, "debugger continued");
      dbg._controller.activeThread.addOneTimeListener("paused", () => {
        interruptDeferred.resolve();
      });
      let btnDoit = options.window.document.querySelector("input[type=button]");
      EventUtils.sendMouseEvent({ type:"click" }, btnDoit);
    });
    helpers.audit(options, [{
      setup: "dbg continue",
      exec: { output: "" }
    }]);
    yield interruptDeferred.promise;

    yield cmd("dbg step in");
    yield cmd("dbg step in");
    yield cmd("dbg step in");
    let output = options.window.document.querySelector("input[type=text]");
    is(output.value, "step in", "debugger stepped in");
    yield cmd("dbg step over");
    is(output.value, "step over", "debugger stepped over");
    yield cmd("dbg step out");
    is(output.value, "step out", "debugger stepped out");
    yield cmd("dbg continue");
    is(output.value, "dbg continue", "debugger continued");

    let closeDebugger = function() {
      let closeDeferred = promise.defer();
      helpers.audit(options, [{
        setup: "dbg close",
        completed: false,
        exec: { output: "" }
      }]).then(function() {
        let closeToolbox = gDevTools.getToolbox(options.target);
        if (!closeToolbox) {
          ok(true, "Debugger is closed.");
          closeDeferred.resolve();
        } else {
          closeToolbox.on("destroyed", () => {
            ok(true, "Debugger just closed.");
            closeDeferred.resolve();
          });
        }
      });
      return closeDeferred.promise;
    };

    // We close the debugger twice to ensure 'dbg close' doesn't error when
    // toolbox is already closed. See bug 884638 for more info.
    yield closeDebugger();
    yield closeDebugger();
    yield helpers.closeToolbar(options);
    yield helpers.closeTab(options);
  }).then(finish, helpers.handleError);
}
