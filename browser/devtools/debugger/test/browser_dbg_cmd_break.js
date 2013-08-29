/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the break command works as it should

const TEST_URI = "http://example.com/browser/browser/devtools/debugger/" +
                 "test/browser_dbg_cmd_break.html";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    // To help us run later commands, and clear up after ourselves
    let client, line0;

    return helpers.audit(options, [
      {
        setup: 'break',
        check: {
          input:  'break',
          hints:       ' add line',
          markup: 'IIIII',
          status: 'ERROR',
        },
      },
      {
        setup: 'break add',
        check: {
          input:  'break add',
          hints:           ' line',
          markup: 'IIIIIVIII',
          status: 'ERROR'
        },
      },
      {
        setup: 'break add line',
        check: {
          input:  'break add line',
          hints:                ' <file> <line>',
          markup: 'VVVVVVVVVVVVVV',
          status: 'ERROR'
        },
      },
      {
        name: 'open toolbox',
        setup: function() {
          var deferred = promise.defer();

          var openDone = gDevTools.showToolbox(options.target, "jsdebugger");
          openDone.then(function(toolbox) {
            let dbg = toolbox.getCurrentPanel();
            ok(dbg, "DebuggerPanel exists");

            // Wait for the initial resume...
            dbg.panelWin.gClient.addOneTimeListener("resumed", function() {
              info("Starting tests");

              client = dbg.panelWin.gClient;
              client.activeThread.addOneTimeListener("framesadded", function() {
                line0 = '' + options.window.wrappedJSObject.line0;
                deferred.resolve();
              });

              // Trigger newScript notifications using eval.
              content.wrappedJSObject.firstCall();
            });
          });

          return deferred.promise;
        },
        post: function() {
          ok(client, "Debugger client exists");
          is(line0, 10, "line0 is 10");
        },
      },
      {
        name: 'break add line .../browser_dbg_cmd_break.html 10',
        setup: function() {
          // We have to setup in a function to allow line0 to be initialized
          let line = 'break add line ' + TEST_URI + ' ' + line0;
          return helpers.setInput(options, line);
        },
        check: {
          hints: '',
          status: 'VALID',
          message: '',
          args: {
            file: { value: TEST_URI, message: '' },
            line: { value: 10 }
          }
        },
        exec: {
          output: 'Added breakpoint',
          completed: false
        },
      },
      {
        setup: 'break add line http://example.com/browser/browser/devtools/debugger/test/browser_dbg_cmd_break.html 13',
        check: {
          hints: '',
          status: 'VALID',
          message: '',
          args: {
            file: { value: TEST_URI, message: '' },
            line: { value: 13 }
          }
        },
        exec: {
          output: 'Added breakpoint',
          completed: false
        },
      },
      {
        setup: 'break list',
        check: {
          input:  'break list',
          hints:            '',
          markup: 'VVVVVVVVVV',
          status: 'VALID'
        },
        exec: {
          output: [
            /Source/, /Remove/,
            /cmd_break\.html:10/,
            /cmd_break\.html:13/
          ]
        },
      },
      {
        name: 'cleanup',
        setup: function() {
          // a.k.a "return client.activeThread.resume();"
          var deferred = promise.defer();
          client.activeThread.resume(function() {
            deferred.resolve();
          });
          return deferred.promise;
        },
      },
      {
        setup: 'break del 0',
        check: {
          input:  'break del 0',
          hints:             ' -> browser_dbg_cmd_break.html:10',
          markup: 'VVVVVVVVVVI',
          status: 'ERROR',
          args: {
            breakpoint: {
              status: 'INCOMPLETE',
              message: ''
            },
          }
        },
      },
      {
        setup: 'break del browser_dbg_cmd_break.html:10',
        check: {
          input:  'break del browser_dbg_cmd_break.html:10',
          hints:                                         '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            breakpoint: { arg: ' browser_dbg_cmd_break.html:10' },
          }
        },
        exec: {
          output: 'Breakpoint removed',
          completed: false
        },
      },
      {
        setup: 'break list',
        check: {
          input:  'break list',
          hints:            '',
          markup: 'VVVVVVVVVV',
          status: 'VALID'
        },
        exec: {
          output: [
            /Source/, /Remove/,
            /browser_dbg_cmd_break\.html:13/
          ]
        },
      },
      {
        setup: 'break del browser_dbg_cmd_break.html:13',
        check: {
          input:  'break del browser_dbg_cmd_break.html:13',
          hints:                                         '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            breakpoint: { arg: ' browser_dbg_cmd_break.html:13' },
          }
        },
        exec: {
          output: 'Breakpoint removed',
          completed: false
        },
      },
      {
        setup: 'break list',
        check: {
          input:  'break list',
          hints:            '',
          markup: 'VVVVVVVVVV',
          status: 'VALID'
        },
        exec: {
          output: 'No breakpoints set'
        },
        post: function() {
          client = undefined;

          let toolbox = gDevTools.getToolbox(options.target);
          return toolbox.destroy();
        }
      },
    ]);
  }).then(finish);
}
