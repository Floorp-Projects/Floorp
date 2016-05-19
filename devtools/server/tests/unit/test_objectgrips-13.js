/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that ObjectClient.prototype.getDefinitionSite and the "definitionSite"
// request work properly.

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  Components.utils.evalInSandbox(function stopMe() {
    debugger;
  }.toString(), gDebuggee);

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-grips", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      add_pause_listener();
    });
  });
  do_test_pending();
}

function add_pause_listener()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    const [funcGrip, objGrip] = aPacket.frame.arguments;
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
    do_check_true(!error);
    do_check_eq(source.url, getFilePath("test_objectgrips-13.js"));
    do_check_eq(line, gDebuggee.line0 + 1);
    do_check_eq(column, 0);

    test_bad_definition_site(obj);
  });
}

function test_bad_definition_site(obj) {
  try {
    obj._client.request("definitionSite", () => do_check_true(false));
  } catch (e) {
    gThreadClient.resume(() => finishClient(gClient));
  }
}
