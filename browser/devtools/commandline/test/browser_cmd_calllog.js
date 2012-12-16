/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the calllog commands works as they should

let imported = {};
Components.utils.import("resource:///modules/HUDService.jsm", imported);

const TEST_URI = "data:text/html;charset=utf-8,gcli-calllog";

function test() {
  DeveloperToolbarTest.test(TEST_URI, [ testCallLogStatus, testCallLogExec ]);
}

function testCallLogStatus() {
  helpers.setInput('calllog');
  helpers.check({
    input:  'calllog',
    hints:         '',
    markup: 'IIIIIII',
    status: 'ERROR'
  });

  helpers.setInput('calllog start');
  helpers.check({
    input:  'calllog start',
    hints:               '',
    markup: 'VVVVVVVVVVVVV',
    status: 'VALID'
  });

  helpers.setInput('calllog stop');
  helpers.check({
    input:  'calllog stop',
    hints:              '',
    markup: 'VVVVVVVVVVVV',
    status: 'VALID'
  });
}

function testCallLogExec() {
  DeveloperToolbarTest.exec({
    typed: "calllog stop",
    args: { },
    outputMatch: /No call logging/,
  });

  let hud = null;
  var onWebConsoleOpen = DeveloperToolbarTest.checkCalled(function(aSubject) {
    Services.obs.removeObserver(onWebConsoleOpen, "web-console-created");

    aSubject.QueryInterface(Ci.nsISupportsString);
    hud = imported.HUDService.getHudReferenceById(aSubject.data);
    ok(hud.hudId in imported.HUDService.hudReferences, "console open");

    DeveloperToolbarTest.exec({
      typed: "calllog stop",
      args: { },
      outputMatch: /Stopped call logging/,
    });

    DeveloperToolbarTest.exec({
      typed: "console clear",
      args: {},
      blankOutput: true,
    });

    let labels = hud.outputNode.querySelectorAll(".webconsole-msg-output");
    is(labels.length, 0, "no output in console");

    DeveloperToolbarTest.exec({
      typed: "console close",
      args: {},
      blankOutput: true,
      completed: false,
    });
  });

  Services.obs.addObserver(onWebConsoleOpen, "web-console-created", false);

  DeveloperToolbarTest.exec({
    typed: "calllog start",
    args: { },
    outputMatch: /Call logging started/,
  });
}
