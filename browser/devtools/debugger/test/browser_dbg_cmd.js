/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  const TEST_URI = "http://example.com/browser/browser/devtools/debugger/" +
                   "test/browser_dbg_cmd.html";

  helpers.addTabWithToolbar(TEST_URI, function(options) {
    let deferred = promise.defer();

    let openDone = helpers.audit(options, [{
      setup: "dbg open",
      exec: { output: "", completed: false }
    }]);

    openDone.then(function() {
      gDevTools.showToolbox(options.target, "jsdebugger").then(function(toolbox) {
        let dbg = toolbox.getCurrentPanel();
        ok(dbg, "DebuggerPanel exists");

        function cmd(typed, callback) {
          dbg._controller.activeThread.addOneTimeListener("paused", callback);
          helpers.audit(options, [{
            setup: typed,
            exec: { output: "" }
          }]);
        }

        // Wait for the initial resume...
        dbg.panelWin.gClient.addOneTimeListener("resumed", function() {
          info("Starting tests");

          let contentDoc = content.window.document;
          let output = contentDoc.querySelector("input[type=text]");
          let btnDoit = contentDoc.querySelector("input[type=button]");

          helpers.audit(options, [{
            setup: "dbg list",
            exec: { output: /browser_dbg_cmd.html/ }
          }]);

          cmd("dbg interrupt", function() {
            ok(true, "debugger is paused");
            dbg._controller.activeThread.addOneTimeListener("resumed", function() {
              ok(true, "debugger continued");
              dbg._controller.activeThread.addOneTimeListener("paused", function() {
                cmd("dbg step in", function() {
                  cmd("dbg step in", function() {
                    cmd("dbg step in", function() {
                      is(output.value, "step in", "debugger stepped in");
                      cmd("dbg step over", function() {
                        is(output.value, "step over", "debugger stepped over");
                        cmd("dbg step out", function() {
                          is(output.value, "step out", "debugger stepped out");
                          cmd("dbg continue", function() {
                            is(output.value, "dbg continue", "debugger continued");

                            function closeDebugger(cb) {
                              helpers.audit(options, [{
                                setup: "dbg close",
                                completed: false,
                                exec: { output: "" }
                              }]);

                              let toolbox = gDevTools.getToolbox(options.target);
                              if (!toolbox) {
                                ok(true, "Debugger was closed.");
                                cb();
                              } else {
                                toolbox.on("destroyed", function () {
                                  ok(true, "Debugger was closed.");
                                  cb();
                                });
                              }
                            }

                            // We're closing the debugger twice to make sure
                            // 'dbg close' doesn't error when toolbox is already
                            // closed. See bug 884638 for more info.

                            closeDebugger(() => {
                              closeDebugger(() => deferred.resolve());
                            });
                          });
                        });
                      });
                    });
                  });
                });
              });
              EventUtils.sendMouseEvent({type:"click"}, btnDoit);
            });

            helpers.audit(options, [{
              setup: "dbg continue",
              exec: { output: "" }
            }]);
          });
        });
      });
    });

    return deferred.promise;
  }).then(finish);
}
