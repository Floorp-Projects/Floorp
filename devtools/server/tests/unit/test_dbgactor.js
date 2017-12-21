/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gClient;
var gDebuggee;

const xpcInspector = Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);

function run_test() {
  initTestDebuggerServer();
  gDebuggee = testGlobal("test-1");
  DebuggerServer.addTestGlobal(gDebuggee);

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.addListener("connected", function (event, type, traits) {
    gClient.listTabs((response) => {
      Assert.ok("tabs" in response);
      for (let tab of response.tabs) {
        if (tab.title == "test-1") {
          test_attach_tab(tab.actor);
          return false;
        }
      }
      // We should have found our tab in the list.
      Assert.ok(false);
      return undefined;
    });
  });

  gClient.connect();

  do_test_pending();
}

// Attach to |tabActor|, and check the response.
function test_attach_tab(tabActor) {
  gClient.request({ to: tabActor, type: "attach" }, function (response) {
    Assert.equal(false, "error" in response);
    Assert.equal(response.from, tabActor);
    Assert.equal(response.type, "tabAttached");
    Assert.ok(typeof response.threadActor === "string");

    test_attach_thread(response.threadActor);
  });
}

// Attach to |threadActor|, check the response, and resume it.
function test_attach_thread(threadActor) {
  gClient.request({ to: threadActor, type: "attach" }, function (response) {
    Assert.equal(false, "error" in response);
    Assert.equal(response.from, threadActor);
    Assert.equal(response.type, "paused");
    Assert.ok("why" in response);
    Assert.equal(response.why.type, "attached");

    test_resume_thread(threadActor);
  });
}

// Resume |threadActor|, and see that it stops at the 'debugger'
// statement.
function test_resume_thread(threadActor) {
  // Allow the client to resume execution.
  gClient.request({ to: threadActor, type: "resume" }, function (response) {
    Assert.equal(false, "error" in response);
    Assert.equal(response.from, threadActor);
    Assert.equal(response.type, "resumed");

    Assert.equal(xpcInspector.eventLoopNestLevel, 0);

    // Now that we know we're resumed, we can make the debuggee do something.
    Cu.evalInSandbox("var a = true; var b = false; debugger; var b = true;", gDebuggee);
    // Now make sure that we've run the code after the debugger statement...
    Assert.ok(gDebuggee.b);
  });

  gClient.addListener("paused", function (name, packet) {
    Assert.equal(name, "paused");
    Assert.equal(false, "error" in packet);
    Assert.equal(packet.from, threadActor);
    Assert.equal(packet.type, "paused");
    Assert.ok("actor" in packet);
    Assert.ok("why" in packet);
    Assert.equal(packet.why.type, "debuggerStatement");

    // Reach around the protocol to check that the debuggee is in the state
    // we expect.
    Assert.ok(gDebuggee.a);
    Assert.ok(!gDebuggee.b);

    Assert.equal(xpcInspector.eventLoopNestLevel, 1);

    // Let the debuggee continue execution.
    gClient.request({ to: threadActor, type: "resume" }, cleanup);
  });
}

function cleanup() {
  gClient.addListener("closed", function (event, result) {
    do_test_finished();
  });

  try {
    let inspector = Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);
    Assert.equal(inspector.eventLoopNestLevel, 0);
  } catch (e) {
    dump(e);
  }

  gClient.close();
}
