function test() {
  const TEST_URI = TEST_BASE_HTTP + "resources_dbg.html";

  DeveloperToolbarTest.test(TEST_URI, function GAT_test() {
    let pane = DebuggerUI.toggleDebugger();
    ok(pane, "toggleDebugger() should return a pane.");
    let frame = pane._frame;

    frame.addEventListener("Debugger:Connecting", function dbgConnected(aEvent) {
      frame.removeEventListener("Debugger:Connecting", dbgConnected, true);

      // Wait for the initial resume...
      aEvent.target.ownerDocument.defaultView.gClient
          .addOneTimeListener("resumed", function() {

        info("Starting tests.");

        let contentDoc = content.window.document;
        let output = contentDoc.querySelector("input[type=text]");
        let btnDoit = contentDoc.querySelector("input[type=button]");

        cmd("dbg interrupt", function() {
          ok(true, "debugger is paused");
          pane.contentWindow.gClient.addOneTimeListener("resumed", function() {
            ok(true, "debugger continued");
            pane.contentWindow.gClient.addOneTimeListener("paused", function() {
              cmd("dbg step in", function() {
                cmd("dbg step in", function() {
                  cmd("dbg step in", function() {
                    is(output.value, "step in", "debugger stepped in");
                    cmd("dbg step over", function() {
                      is(output.value, "step over", "debugger stepped over");
                      cmd("dbg step out", function() {
                        is(output.value, "step out", "debugger stepped out");
                        cmd("dbg continue", function() {
                          cmd("dbg continue", function() {
                            is(output.value, "dbg continue", "debugger continued");
                            pane.contentWindow.gClient.close(function() {
                              finish();
                            });
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
          DeveloperToolbarTest.exec({
            typed: "dbg continue",
            blankOutput: true
          });
        });
      });

      function cmd(aTyped, aCallback) {
        pane.contentWindow.gClient.addOneTimeListener("paused", aCallback);
        DeveloperToolbarTest.exec({
          typed: aTyped,
          blankOutput: true
        });
      }
    });
  });
}
