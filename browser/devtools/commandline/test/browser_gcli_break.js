/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the break command works as it should

const TEST_URI = "http://example.com/browser/browser/devtools/commandline/test/browser_gcli_break.html";

function test() {
  DeveloperToolbarTest.test(TEST_URI, function(browser, tab) {
    testBreakCommands();
  });
}

function testBreakCommands() {
  DeveloperToolbarTest.checkInputStatus({
    typed: "brea",
    directTabText: "k",
    status: "ERROR"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "break",
    status: "ERROR"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "break add",
    status: "ERROR"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "break add line",
    emptyParameters: [ " <file>", " <line>" ],
    status: "ERROR"
  });

  let pane = DebuggerUI.toggleDebugger();
  pane._frame.addEventListener("Debugger:Connecting", function dbgConnected() {
    pane._frame.removeEventListener("Debugger:Connecting", dbgConnected, true);

    // Wait for the initial resume.
    let client = pane.contentWindow.gClient;
    client.addOneTimeListener("resumed", function() {
      client.activeThread.addOneTimeListener("framesadded", function() {
        DeveloperToolbarTest.checkInputStatus({
          typed: "break add line " + TEST_URI + " " + content.wrappedJSObject.line0,
          status: "VALID"
        });
        DeveloperToolbarTest.exec({
          args: {
            type: 'line',
            file: TEST_URI,
            line: content.wrappedJSObject.line0
          },
          completed: false
        });

        DeveloperToolbarTest.checkInputStatus({
          typed: "break list",
          status: "VALID"
        });
        DeveloperToolbarTest.exec();

        client.activeThread.resume(function() {
          DeveloperToolbarTest.checkInputStatus({
            typed: "break del 0",
            status: "VALID"
          });
          DeveloperToolbarTest.exec({
            args: { breakid: 0 },
            completed: false
          });

          finish();
        });
      });

      // Trigger newScript notifications using eval.
      content.wrappedJSObject.firstCall();
    });
  }, true);
}
