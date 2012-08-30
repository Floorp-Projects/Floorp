/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the edit command works

const TEST_URI = "http://example.com/browser/browser/devtools/styleeditor/" +
                 "test/browser_styleeditor_cmd_edit.html";

function test() {
  DeveloperToolbarTest.test(TEST_URI, [ testEditStatus ]);
  // Bug 759853
  // testEditExec
}

function testEditStatus(browser, tab) {
  helpers.setInput('edit');
  helpers.check({
    input:  'edit',
    hints:      ' <resource> [line]',
    markup: 'VVVV',
    status: 'ERROR',
    args: {
      resource: { status: 'INCOMPLETE' },
      line: { status: 'VALID' },
    }
  });

  helpers.setInput('edit i');
  helpers.check({
    input:  'edit i',
    hints:        'nline-css [line]',
    markup: 'VVVVVI',
    status: 'ERROR',
    args: {
      resource: { arg: ' i', status: 'INCOMPLETE' },
      line: { status: 'VALID' },
    }
  });

  helpers.setInput('edit c');
  helpers.check({
    input:  'edit c',
    hints:        'ss#style2 [line]',
    markup: 'VVVVVI',
    status: 'ERROR',
    args: {
      resource: { arg: ' c', status: 'INCOMPLETE' },
      line: { status: 'VALID' },
    }
  });

  helpers.setInput('edit http');
  helpers.check({
    input:  'edit http',
    hints:           '://example.com/browser/browser/devtools/styleeditor/test/resources_inpage1.css [line]',
    markup: 'VVVVVIIII',
    status: 'ERROR',
    args: {
      resource: { arg: ' http', status: 'INCOMPLETE', message: '' },
      line: { status: 'VALID' },
    }
  });

  helpers.setInput('edit page1');
  helpers.check({
    input:  'edit page1',
    hints:            ' [line] -> http://example.com/browser/browser/devtools/styleeditor/test/resources_inpage1.css',
    markup: 'VVVVVIIIII',
    status: 'ERROR',
    args: {
      resource: { arg: ' page1', status: 'INCOMPLETE', message: '' },
      line: { status: 'VALID' },
    }
  });

  helpers.setInput('edit page2');
  helpers.check({
    input:  'edit page2',
    hints:            ' [line] -> http://example.com/browser/browser/devtools/styleeditor/test/resources_inpage2.css',
    markup: 'VVVVVIIIII',
    status: 'ERROR',
    args: {
      resource: { arg: ' page2', status: 'INCOMPLETE', message: '' },
      line: { status: 'VALID' },
    }
  });

  helpers.setInput('edit stylez');
  helpers.check({
    input:  'edit stylez',
    hints:             ' [line]',
    markup: 'VVVVVEEEEEE',
    status: 'ERROR',
    args: {
      resource: { arg: ' stylez', status: 'ERROR', message: 'Can\'t use \'stylez\'.' },
      line: { status: 'VALID' },
    }
  });

  helpers.setInput('edit css#style2');
  helpers.check({
    input:  'edit css#style2',
    hints:                 ' [line]',
    markup: 'VVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      resource: { arg: ' css#style2', status: 'VALID', message: '' },
      line: { status: 'VALID' },
    }
  });

  helpers.setInput('edit css#style2 5');
  helpers.check({
    input:  'edit css#style2 5',
    hints:                   '',
    markup: 'VVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      resource: { arg: ' css#style2', status: 'VALID', message: '' },
      line: { value: 5, arg: ' 5', status: 'VALID' },
    }
  });

  helpers.setInput('edit css#style2 0');
  helpers.check({
    input:  'edit css#style2 0',
    hints:                   '',
    markup: 'VVVVVVVVVVVVVVVVE',
    status: 'ERROR',
    args: {
      resource: { arg: ' css#style2', status: 'VALID', message: '' },
      line: { arg: ' 0', status: 'ERROR', message: '0 is smaller than minimum allowed: 1.' },
    }
  });

  helpers.setInput('edit css#style2 -1');
  helpers.check({
    input:  'edit css#style2 -1',
    hints:                    '',
    markup: 'VVVVVVVVVVVVVVVVEE',
    status: 'ERROR',
    args: {
      resource: { arg: ' css#style2', status: 'VALID', message: '' },
      line: { arg: ' -1', status: 'ERROR', message: '-1 is smaller than minimum allowed: 1.' },
    }
  });
}

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
      finish();
    }, false);
  },
  onCloseWindow: function(win) { },
  onWindowTitleChange: function(win, title) { }
};

function testEditExec(browser, tab) {

  Services.wm.addListener(windowListener);

  var style2 = browser.contentDocument.getElementById("style2");
  DeveloperToolbarTest.exec({
    typed: "edit css#style2",
    args: {
      resource: function(resource) {
        return resource.element.ownerNode == style2;
      },
      line: 1
    },
    completed: true,
    blankOutput: true,
  });
}
