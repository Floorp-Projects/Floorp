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
          hints:       '',
          markup: 'IIIII',
          status: 'ERROR',
        },
      },
      {
        setup: 'break add',
        check: {
          input:  'break add',
          hints:           '',
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
          var deferred = Promise.defer();

          var openDone = gDevTools.showToolbox(options.target, "jsdebugger");
          openDone.then(function(toolbox) {
            let dbg = toolbox.getCurrentPanel();
            ok(dbg, "DebuggerPanel exists");
            dbg.once("connected", function() {
              // Wait for the initial resume...
              dbg.panelWin.gClient.addOneTimeListener("resumed", function() {
                dbg._view.Variables.lazyEmpty = false;

                client = dbg.panelWin.gClient;
                client.activeThread.addOneTimeListener("framesadded", function() {
                  line0 = '' + options.window.wrappedJSObject.line0;
                  deferred.resolve();
                });

                // Trigger newScript notifications using eval.
                content.wrappedJSObject.firstCall();
              });
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
            line: { value: 10 }, // would like to use line0, but see above
                                 // if this proves to be too fragile, disable
          }
        },
        exec: {
          output: '',
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
          output: ''
        },
      },
      {
        name: 'cleanup',
        setup: function() {
          var deferred = Promise.defer();
          client.activeThread.resume(function() {
            deferred.resolve();
          });
          return deferred.promise;
        },
      },
      {
        setup: 'break del 9',
        check: {
          input:  'break del 9',
          hints:             '',
          markup: 'VVVVVVVVVVE',
          status: 'ERROR',
          args: {
            breakid: {
              status: 'ERROR',
              message: '9 is greater than maximum allowed: 0.'
            },
          }
        },
      },
      {
        setup: 'break del 0',
        check: {
          input:  'break del 0',
          hints:             '',
          markup: 'VVVVVVVVVVV',
          status: 'VALID',
          args: {
            breakid: { value: 0 },
          }
        },
        exec: {
          output: '',
          completed: false
        },
      },
    ]);
  }).then(finish);
}
