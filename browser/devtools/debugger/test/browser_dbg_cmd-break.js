/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the break commands works as they should.
 */

const TAB_URL = EXAMPLE_URL + "doc_cmd-break.html";
let TAB_URL_ACTOR;

function test() {
  let gPanel, gDebugger, gThreadClient, gSources;
  let gLineNumber;

  let expectedActorObj = {
    value: null,
    message: ''
  };

  helpers.addTabWithToolbar(TAB_URL, aOptions => {
    return Task.spawn(function*() {
      yield helpers.audit(aOptions, [{
        setup: 'break',
        check: {
          input:  'break',
          hints:       ' add line',
          markup: 'IIIII',
          status: 'ERROR',
        }
      }]);

      yield helpers.audit(aOptions, [{
        setup: 'break add',
        check: {
          input:  'break add',
          hints:           ' line',
          markup: 'IIIIIVIII',
          status: 'ERROR'
        }
      }]);

      yield helpers.audit(aOptions, [{
        setup: 'break add line',
        check: {
          input:  'break add line',
          hints:                ' <file> <line>',
          markup: 'VVVVVVVVVVVVVV',
          status: 'ERROR'
        }
      }]);

      yield helpers.audit(aOptions, [{
        name: 'open toolbox',
        setup: function() {
          return initDebugger(gBrowser.selectedTab).then(([aTab, aDebuggee, aPanel]) => {
            // Spin the event loop before causing the debuggee to pause, to allow
            // this function to return first.
            executeSoon(() => aDebuggee.firstCall());

            return waitForSourceAndCaretAndScopes(aPanel, ".html", 1).then(() => {
              gPanel = aPanel;
              gDebugger = gPanel.panelWin;
              gThreadClient = gPanel.panelWin.gThreadClient;
              gLineNumber = '' + aOptions.window.wrappedJSObject.gLineNumber;
              gSources = gDebugger.DebuggerView.Sources;

              expectedActorObj.value = getSourceActor(gSources, TAB_URL);
            });
          });
        },
        post: function() {
          ok(gThreadClient, "Debugger client exists.");
          is(gLineNumber, 14, "gLineNumber is correct.");
        },
      }]);

      yield helpers.audit(aOptions, [{
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
            file: expectedActorObj,
            line: { value: 14 }
          }
        },
        exec: {
          output: 'Added breakpoint'
        }
      }]);

      yield helpers.audit(aOptions, [{
        setup: 'break add line ' + TAB_URL + ' 17',
        check: {
          hints: '',
          status: 'VALID',
          message: '',
          args: {
            file: expectedActorObj,
            line: { value: 17 }
          }
        },
        exec: {
          output: 'Added breakpoint'
        }
      }]);

      yield helpers.audit(aOptions, [{
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
      }]);

      yield helpers.audit(aOptions, [{
        name: 'cleanup',
        setup: function() {
          let deferred = promise.defer();
          gThreadClient.resume(deferred.resolve);
          return deferred.promise;
        }
      }]);

      yield helpers.audit(aOptions, [{
        setup: 'break del 14',
        check: {
          input:  'break del 14',
          hints:              ' -> doc_cmd-break.html:14',
          markup: 'VVVVVVVVVVII',
          status: 'ERROR',
          args: {
            breakpoint: {
              status: 'INCOMPLETE',
              message: 'Value required for \'breakpoint\'.'
            }
          }
        }
      }]);

      yield helpers.audit(aOptions, [{
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
          output: 'Breakpoint removed'
        }
      }]);

      yield helpers.audit(aOptions, [{
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
      }]);

      yield helpers.audit(aOptions, [{
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
          output: 'Breakpoint removed'
        }
      }]);

      yield helpers.audit(aOptions, [{
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
      }]);
    });
  }).then(finish);
}
