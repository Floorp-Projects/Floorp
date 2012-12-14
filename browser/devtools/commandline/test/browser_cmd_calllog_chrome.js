/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the calllog commands works as they should

let imported = {};
Components.utils.import("resource:///modules/HUDService.jsm", imported);

const TEST_URI = "data:text/html;charset=utf-8,cmd-calllog-chrome";

function test() {
  DeveloperToolbarTest.test(TEST_URI, function CLCTest(browser, tab) {
    testCallLogStatus();
    testCallLogExec();
  });
}

function testCallLogStatus() {
  DeveloperToolbarTest.checkInputStatus({
    typed: "calllog",
    status: "ERROR",
    emptyParameters: [ " " ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "calllog chromestart content-variable window",
    status: "VALID",
    emptyParameters: [ " " ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "calllog chromestop",
    status: "VALID",
    emptyParameters: [ " " ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "calllog chromestart chrome-variable window",
    status: "VALID",
    emptyParameters: [ " " ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "calllog chromestop",
    status: "VALID",
    emptyParameters: [ " " ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "calllog chromestart javascript \"({a1: function() {this.a2()}," +
           "a2: function() {}});\"",
    status: "VALID",
    emptyParameters: [ " " ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "calllog chromestop",
    status: "VALID",
    emptyParameters: [ " " ]
  });
}

function testCallLogExec() {
  DeveloperToolbarTest.exec({
    typed: "calllog chromestop",
    args: { },
    outputMatch: /No call logging/,
  });

  function onWebConsoleOpen(aSubject) {
    Services.obs.removeObserver(onWebConsoleOpen, "web-console-created");

    aSubject.QueryInterface(Ci.nsISupportsString);
    let hud = imported.HUDService.getHudReferenceById(aSubject.data);
    ok(hud.hudId in imported.HUDService.hudReferences, "console open");

    DeveloperToolbarTest.exec({
      typed: "calllog chromestop",
      args: { },
      outputMatch: /Stopped call logging/,
    });

    DeveloperToolbarTest.exec({
      typed: "calllog chromestart javascript XXX",
      outputMatch: /following exception/,
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
      completed: false,
    });

    executeSoon(finish);
  }
  Services.obs.addObserver(onWebConsoleOpen, "web-console-created", false);

  DeveloperToolbarTest.exec({
    typed: "calllog chromestart javascript \"({a1: function() {this.a2()},a2: function() {}});\"",
    outputMatch: /Call logging started/,
    completed: false,
  });
}
