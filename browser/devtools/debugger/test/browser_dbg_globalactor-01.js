/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check extension-added global actor API.
 */

var gClient = null;

function test()
{
  DebuggerServer.addActors("chrome://mochitests/content/browser/browser/devtools/debugger/test/testactors.js");

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(function(aType, aTraits) {
    is(aType, "browser", "Root actor should identify itself as a browser.");
    gClient.listTabs(function(aResponse) {
      let globalActor = aResponse.testGlobalActor1;
      ok(globalActor, "Found the test tab actor.")
      ok(globalActor.indexOf("testone") >= 0,
         "testTabActor's actorPrefix should be used.");
      gClient.request({ to: globalActor, type: "ping" }, function(aResponse) {
        is(aResponse.pong, "pong", "Actor should respond to requests.");
        finish_test();
      });
    });
  });
}

function finish_test()
{
  gClient.close(function() {
    finish();
  });
}
