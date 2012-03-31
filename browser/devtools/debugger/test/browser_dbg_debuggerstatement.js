/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the behavior of the debugger statement.

var gClient = null;
var gTab = null;
const DEBUGGER_TAB_URL = EXAMPLE_URL + "browser_dbg_debuggerstatement.html";

function test()
{
  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(function(aType, aTraits) {
    gTab = addTab(DEBUGGER_TAB_URL, function() {
      attach_tab_actor_for_url(gClient, DEBUGGER_TAB_URL, function(actor, response) {
        test_early_debugger_statement(response);
      });
    });
  });
}

function test_early_debugger_statement(aActor)
{
  let paused = function(aEvent, aPacket) {
    ok(false, "Pause shouldn't be called before we've attached!\n");
    finish_test();
  };
  gClient.addListener("paused", paused);
  // This should continue without nesting an event loop and calling
  // the onPaused hook, because we haven't attached yet.
  gTab.linkedBrowser.contentWindow.wrappedJSObject.runDebuggerStatement();

  gClient.removeListener("paused", paused);

  // Now attach and resume...
  gClient.request({ to: aActor.threadActor, type: "attach" }, function(aResponse) {
    gClient.request({ to: aActor.threadActor, type: "resume" }, function(aResponse) {
      test_debugger_statement(aActor);
    });
  });
}

function test_debugger_statement(aActor)
{
  var stopped = false;
  gClient.addListener("paused", function(aEvent, aPacket) {
    stopped = true;

    gClient.request({ to: aActor.threadActor, type: "resume" }, function() {
      finish_test();
    });
  });

  // Reach around the debugging protocol and execute the debugger
  // statement.
  gTab.linkedBrowser.contentWindow.wrappedJSObject.runDebuggerStatement();
  ok(stopped, "Should trigger the pause handler on a debugger statement.");
}

function finish_test()
{
  removeTab(gTab);
  gClient.close(function() {
    finish();
  });
}
