/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the edit command works

const TEST_URI = "http://example.com/browser/browser/devtools/styleeditor/" +
                 "test/browser_styleeditor_cmd_edit.html";


function test() {
  let windowClosed = Promise.defer();

  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.audit(options, [
      {
        setup: "edit",
        check: {
          input:  'edit',
          hints:      ' <resource> [line]',
          markup: 'VVVV',
          status: 'ERROR',
          args: {
            resource: { status: 'INCOMPLETE' },
            line: { status: 'VALID' },
          }
        },
      },
      {
        setup: "edit i",
        check: {
          input:  'edit i',
          hints:        'nline-css [line]',
          markup: 'VVVVVI',
          status: 'ERROR',
          args: {
            resource: { arg: ' i', status: 'INCOMPLETE' },
            line: { status: 'VALID' },
          }
        },
      },
      {
        setup: "edit c",
        check: {
          input:  'edit c',
          hints:        'ss#style2 [line]',
          markup: 'VVVVVI',
          status: 'ERROR',
          args: {
            resource: { arg: ' c', status: 'INCOMPLETE' },
            line: { status: 'VALID' },
          }
        },
      },
      {
        setup: "edit http",
        check: {
          input:  'edit http',
          hints:           '://example.com/browser/browser/devtools/styleeditor/test/resources_inpage1.css [line]',
          markup: 'VVVVVIIII',
          status: 'ERROR',
          args: {
            resource: { arg: ' http', status: 'INCOMPLETE', message: '' },
            line: { status: 'VALID' },
          }
        },
      },
      {
        setup: "edit page1",
        check: {
          input:  'edit page1',
          hints:            ' [line] -> http://example.com/browser/browser/devtools/styleeditor/test/resources_inpage1.css',
          markup: 'VVVVVIIIII',
          status: 'ERROR',
          args: {
            resource: { arg: ' page1', status: 'INCOMPLETE', message: '' },
            line: { status: 'VALID' },
          }
        },
      },
      {
        setup: "edit page2",
        check: {
          input:  'edit page2',
          hints:            ' [line] -> http://example.com/browser/browser/devtools/styleeditor/test/resources_inpage2.css',
          markup: 'VVVVVIIIII',
          status: 'ERROR',
          args: {
            resource: { arg: ' page2', status: 'INCOMPLETE', message: '' },
            line: { status: 'VALID' },
          }
        },
      },
      {
        setup: "edit stylez",
        check: {
          input:  'edit stylez',
          hints:             ' [line]',
          markup: 'VVVVVEEEEEE',
          status: 'ERROR',
          args: {
            resource: { arg: ' stylez', status: 'ERROR', message: 'Can\'t use \'stylez\'.' },
            line: { status: 'VALID' },
          }
        },
      },
      {
        setup: "edit css#style2",
        check: {
          input:  'edit css#style2',
          hints:                 ' [line]',
          markup: 'VVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            resource: { arg: ' css#style2', status: 'VALID', message: '' },
            line: { status: 'VALID' },
          }
        },
      },
      {
        setup: "edit css#style2 5",
        check: {
          input:  'edit css#style2 5',
          hints:                   '',
          markup: 'VVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            resource: { arg: ' css#style2', status: 'VALID', message: '' },
            line: { value: 5, arg: ' 5', status: 'VALID' },
          }
        },
      },
      {
        setup: "edit css#style2 0",
        check: {
          input:  'edit css#style2 0',
          hints:                   '',
          markup: 'VVVVVVVVVVVVVVVVE',
          status: 'ERROR',
          args: {
            resource: { arg: ' css#style2', status: 'VALID', message: '' },
            line: { arg: ' 0', status: 'ERROR', message: '0 is smaller than minimum allowed: 1.' },
          }
        },
      },
      {
        setup: "edit css#style2 -1",
        check: {
          input:  'edit css#style2 -1',
          hints:                    '',
          markup: 'VVVVVVVVVVVVVVVVEE',
          status: 'ERROR',
          args: {
            resource: { arg: ' css#style2', status: 'VALID', message: '' },
            line: { arg: ' -1', status: 'ERROR', message: '-1 is smaller than minimum allowed: 1.' },
          }
        },
      },
      {
        // Bug 759853
        skipIf: true,
        name: "edit exec",
        setup: function() {
          var windowListener = {
            onOpenWindow: function(win) {
              // Wait for the window to finish loading
              let win = win.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowInternal || Ci.nsIDOMWindow);
              win.addEventListener("load", function onLoad() {
                win.removeEventListener("load", onLoad, false);
                win.close();
              }, false);
              win.addEventListener("unload", function onUnload() {
                win.removeEventListener("unload", onUnload, false);
                Services.wm.removeListener(windowListener);
                windowClosed.resolve();
              }, false);
            },
            onCloseWindow: function(win) { },
            onWindowTitleChange: function(win, title) { }
          };

          Services.wm.addListener(windowListener);

          helpers.setInput(options, "edit css#style2");
        },
        check: {
          input: "edit css#style2",
          args: {
            resource: {
              value: function(resource) {
                let style2 = options.window.document.getElementById("style2");
                return resource.element.ownerNode == style2;
              }
            },
            line: { value: 1 },
          },
        },
        exec: {
          output: "",
        },
        post: function() {
          return windowClosed.promise;
        }
      },
    ]);
  }).then(finish);
}
