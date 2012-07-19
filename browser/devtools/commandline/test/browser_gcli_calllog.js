/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the calllog commands works as they should

let imported = {};
Components.utils.import("resource:///modules/HUDService.jsm", imported);

const TEST_URI = "data:text/html;charset=utf-8,gcli-calllog";

function test() {
  DeveloperToolbarTest.test(TEST_URI, function(browser, tab) {
    testCallLogStatus();
    testCallLogExec();
    finish();
  });
}

function testCallLogStatus() {
  DeveloperToolbarTest.checkInputStatus({
    typed: "calllog",
    status: "ERROR"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "calllog start",
    status: "VALID",
    emptyParameters: [ ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "calllog start",
    status: "VALID",
    emptyParameters: [ ]
  });
}

function testCallLogExec() {
  DeveloperToolbarTest.exec({
    typed: "calllog stop",
    args: { },
    outputMatch: /No call logging/,
  });

  DeveloperToolbarTest.exec({
    typed: "calllog start",
    args: { },
    outputMatch: /Call logging started/,
  });

  let hud = imported.HUDService.getHudByWindow(content);
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

  let labels = hud.jsterm.outputNode.querySelectorAll(".webconsole-msg-output");
  is(labels.length, 0, "no output in console");

  DeveloperToolbarTest.exec({
    typed: "console close",
    args: {},
    blankOutput: true,
  });
}
