/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the break commands works as they should.
 */

const TAB_URL = EXAMPLE_URL + "doc_cmd-break.html";

function test() {
  let gPanel, gDebugger, gThreadClient;
  let gLineNumber;

  helpers.addTabWithToolbar(TAB_URL, aOptions => {
    return helpers.audit(aOptions, [
      {
        setup: 'break',
        check: {
          input:  'break',
          hints:       ' add line',
          markup: 'IIIII',
          status: 'ERROR',
        }
      },
      {
        setup: 'break add',
        check: {
          input:  'break add',
          hints:           ' line',
          markup: 'IIIIIVIII',
          status: 'ERROR'
        }
      },
      {
        setup: 'break add line',
        check: {
          input:  'break add line',
          hints:                ' <file> <line>',
          markup: 'VVVVVVVVVVVVVV',
          status: 'ERROR'
        }
      },
      {
        name: 'open toolbox',
        setup: function() {
          return initDebugger(gBrowser.selectedTab).then(([aTab, aDebuggee, aPanel]) => {
            // Spin the event loop before causing the debuggee to pause, to allow
            // this function to return first.
            executeSoon(() => aDebuggee.firstCall());

            return waitForSourceAndCaretAndScopes(aPanel, ".html", 17).then(() => {
              gPanel = aPanel;
              gDebugger = gPanel.panelWin;
              gThreadClient = gPanel.panelWin.gThreadClient;
              gLineNumber = '' + aOptions.window.wrappedJSObject.gLineNumber;
            });
          });
        },
        post: function() {
          ok(gThreadClient, "Debugger client exists.");
          is(gLineNumber, 14, "gLineNumber is correct.");
        },
      },
      {
        name: 'break add line .../doc_cmd-break.html 14',
        setup: function() {
          // We have to setup in a function to allow gLineNumber to be initialized.
          let line = 'break add line ' + TAB_URL + ' ' + gLineNumber;
          return helpers.setInput(aOptions, line);
        },
        check: {
          hints: '',
          status: 'VALID',
          message: '',
          args: {
            file: { value: TAB_URL, message: '' },
            line: { value: 14 }
          }
        },
        exec: {
          output: 'Added breakpoint',
          completed: false
        }
      },
      {
        setup: 'break add line ' + TAB_URL + ' 17',
        check: {
          hints: '',
          status: 'VALID',
          message: '',
          args: {
            file: { value: TAB_URL, message: '' },
            line: { value: 17 }
          }
        },
        exec: {
          output: 'Added breakpoint',
          completed: false
        }
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
            /doc_cmd-break\.html:14/,
            /doc_cmd-break\.html:17/
          ]
        }
      },
      {
        name: 'cleanup',
        setup: function() {
          let deferred = promise.defer();
          gThreadClient.resume(deferred.resolve);
          return deferred.promise;
        }
      },
      {
        setup: 'break del 14',
        check: {
          input:  'break del 14',
          hints:              ' -> doc_cmd-break.html:14',
          markup: 'VVVVVVVVVVII',
          status: 'ERROR',
          args: {
            breakpoint: {
              status: 'INCOMPLETE',
              message: ''
            }
          }
        }
      },
      {
        setup: 'break del doc_cmd-break.html:14',
        check: {
          input:  'break del doc_cmd-break.html:14',
          hints:                                 '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            breakpoint: { arg: ' doc_cmd-break.html:14' },
          }
        },
        exec: {
          output: 'Breakpoint removed',
          completed: false
        }
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
            /doc_cmd-break\.html:17/
          ]
        }
      },
      {
        setup: 'break del doc_cmd-break.html:17',
        check: {
          input:  'break del doc_cmd-break.html:17',
          hints:                                 '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            breakpoint: { arg: ' doc_cmd-break.html:17' },
          }
        },
        exec: {
          output: 'Breakpoint removed',
          completed: false
        }
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
          return teardown(gPanel, { noTabRemoval: true });
        }
      },
    ]);
  }).then(finish);
}
