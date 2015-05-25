/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the debugger commands work as they should.
 */

const TEST_URI = EXAMPLE_URL + "doc_cmd-dbg.html";

function test() {
  return Task.spawn(function*() {
    let options = yield helpers.openTab(TEST_URI);
    yield helpers.openToolbar(options);

    yield helpers.audit(options, [{
      setup: "dbg open",
      exec: { output: "" }
    }]);

    let [gTab, gDebuggee, gPanel] = yield initDebugger(gBrowser.selectedTab);
    let gDebugger = gPanel.panelWin;
    let gThreadClient = gDebugger.gThreadClient;

    yield helpers.audit(options, [{
      setup: "dbg list",
      exec: { output: /doc_cmd-dbg.html/ }
    }]);

    let button = gDebuggee.document.querySelector("input[type=button]");
    let output = gDebuggee.document.querySelector("input[type=text]");

    let cmd = function(aTyped, aState) {
      return promise.all([
        waitForThreadEvents(gPanel, aState),
        helpers.audit(options, [{ setup: aTyped, exec: { output: "" } }])
      ]);
    };

    let click = function(aElement, aState) {
      return promise.all([
        waitForThreadEvents(gPanel, aState),
        executeSoon(() => EventUtils.sendMouseEvent({ type: "click" }, aElement, gDebuggee))
      ]);
    }

    yield cmd("dbg interrupt", "paused");
    is(gThreadClient.state, "paused", "Debugger is paused.");

    yield cmd("dbg continue", "resumed");
    isnot(gThreadClient.state, "paused", "Debugger has continued.");

    yield click(button, "paused");
    is(gThreadClient.state, "paused", "Debugger is paused again.");

    yield cmd("dbg step in", "paused");
    yield cmd("dbg step in", "paused");
    yield cmd("dbg step in", "paused");
    is(output.value, "step in", "Debugger stepped in.");

    yield cmd("dbg step over", "paused");
    is(output.value, "step over", "Debugger stepped over.");

    yield cmd("dbg step out", "paused");
    is(output.value, "step out", "Debugger stepped out.");

    yield cmd("dbg continue", "paused");
    is(output.value, "dbg continue", "Debugger continued.");

    let closeDebugger = function() {
      let deferred = promise.defer();

      helpers.audit(options, [{
        setup: "dbg close",
        exec: { output: "" }
      }])
      .then(() => {
        let toolbox = gDevTools.getToolbox(options.target);
        if (!toolbox) {
          ok(true, "Debugger is closed.");
          deferred.resolve();
        } else {
          toolbox.on("destroyed", () => {
            ok(true, "Debugger just closed.");
            deferred.resolve();
          });
        }
      });

      return deferred.promise;
    };

    // We close the debugger twice to ensure 'dbg close' doesn't error when
    // toolbox is already closed. See bug 884638 for more info.
    yield closeDebugger();
    yield closeDebugger();
    yield helpers.closeToolbar(options);
    yield helpers.closeTab(options);

  }).then(finish, helpers.handleError);
}
