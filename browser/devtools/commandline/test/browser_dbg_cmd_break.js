/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the break command works as it should

const TEST_URI = "http://example.com/browser/browser/devtools/commandline/" +
                 "test/browser_dbg_cmd_break.html";

function test() {
  DeveloperToolbarTest.test(TEST_URI, [ testBreakCommands ]);
}

function testBreakCommands() {
  helpers.setInput('break');
  helpers.check({
    input:  'break',
    hints:       '',
    markup: 'IIIII',
    status: 'ERROR'
  });

  helpers.setInput('break add');
  helpers.check({
    input:  'break add',
    hints:           '',
    markup: 'IIIIIVIII',
    status: 'ERROR'
  });

  helpers.setInput('break add line');
  helpers.check({
    input:  'break add line',
    hints:                ' <file> <line>',
    markup: 'VVVVVVVVVVVVVV',
    status: 'ERROR'
  });

  let pane = DebuggerUI.toggleDebugger();

  var dbgConnected = DeveloperToolbarTest.checkCalled(function() {
    pane._frame.removeEventListener("Debugger:Connecting", dbgConnected, true);

    // Wait for the initial resume.
    let client = pane.contentWindow.gClient;

    var resumed = DeveloperToolbarTest.checkCalled(function() {

      var framesAdded = DeveloperToolbarTest.checkCalled(function() {
        helpers.setInput('break add line ' + TEST_URI + ' ' + content.wrappedJSObject.line0);
        helpers.check({
          hints: '',
          status: 'VALID',
          args: {
            file: { value: TEST_URI },
            line: { value: content.wrappedJSObject.line0 },
          }
        });

        DeveloperToolbarTest.exec({
          args: {
            type: 'line',
            file: TEST_URI,
            line: content.wrappedJSObject.line0
          },
          completed: false
        });

        helpers.setInput('break list');
        helpers.check({
          input:  'break list',
          hints:            '',
          markup: 'VVVVVVVVVV',
          status: 'VALID'
        });

        DeveloperToolbarTest.exec();

        var cleanup = DeveloperToolbarTest.checkCalled(function() {
          helpers.setInput('break del 9');
          helpers.check({
            input:  'break del 9',
            hints:             '',
            markup: 'VVVVVVVVVVE',
            status: 'ERROR',
            args: {
              breakid: { status: 'ERROR', message: '9 is greater than maximum allowed: 0.' },
            }
          });

          helpers.setInput('break del 0');
          helpers.check({
            input:  'break del 0',
            hints:             '',
            markup: 'VVVVVVVVVVV',
            status: 'VALID',
            args: {
              breakid: { value: 0 },
            }
          });

          DeveloperToolbarTest.exec({
            args: { breakid: 0 },
            completed: false
          });
        });

        client.activeThread.resume(cleanup);
      });

      client.activeThread.addOneTimeListener("framesadded", framesAdded);

      // Trigger newScript notifications using eval.
      content.wrappedJSObject.firstCall();
    });

    client.addOneTimeListener("resumed", resumed);
  });

  pane._frame.addEventListener("Debugger:Connecting", dbgConnected, true);
}
