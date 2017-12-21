/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that ObjectClient.prototype.getDefinitionSite and the "definitionSite"
// request work properly.

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  Components.utils.evalInSandbox(function stopMe() {
    debugger;
  }.toString(), gDebuggee);

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-grips",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             add_pause_listener();
                           });
  });
  do_test_pending();
}

function add_pause_listener() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    const [funcGrip, objGrip] = packet.frame.arguments;
    const func = gThreadClient.pauseGrip(funcGrip);
    const obj = gThreadClient.pauseGrip(objGrip);
    test_definition_site(func, obj);
  });

  eval_code();
}

function eval_code() {
  Components.utils.evalInSandbox([
    "this.line0 = Error().lineNumber;",
    "function f() {}",
    "stopMe(f, {});"
  ].join("\n"), gDebuggee);
}

function test_definition_site(func, obj) {
  func.getDefinitionSite(({ error, source, line, column }) => {
    Assert.ok(!error);
    Assert.equal(source.url, getFilePath("test_objectgrips-13.js"));
    Assert.equal(line, gDebuggee.line0 + 1);
    Assert.equal(column, 0);

    test_bad_definition_site(obj);
  });
}

function test_bad_definition_site(obj) {
  try {
    obj._client.request("definitionSite", () => Assert.ok(false));
  } catch (e) {
    gThreadClient.resume(() => finishClient(gClient));
  }
}
