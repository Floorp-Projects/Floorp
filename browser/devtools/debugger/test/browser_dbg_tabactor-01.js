/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check extension-added tab actor lifetimes.
 */

var gTab1 = null;
var gTab1Actor = null;

var gClient = null;

function test()
{
  DebuggerServer.addActors("chrome://mochitests/content/browser/browser/devtools/debugger/test/testactors.js");

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(function (aType, aTraits) {
    is(aType, "browser", "Root actor should identify itself as a browser.");
    get_tab();
  });
}

function get_tab()
{
  gTab1 = addTab(TAB1_URL, function () {
    attach_tab_actor_for_url(gClient, TAB1_URL, function (aGrip) {
      gTab1Actor = aGrip.actor;
      gClient.request({ to: aGrip.actor, type: "testTabActor1" }, function (aResponse) {
        ok(aResponse.actor, "testTabActor1 request should return an actor.");
        ok(aResponse.actor.indexOf("testone") >= 0,
           "testTabActor's actorPrefix should be used.");
        gClient.request({ to: aResponse.actor, type: "ping" }, function (aResponse) {
          is(aResponse.pong, "pong", "Actor should response to requests.");
          finish_test();
        });
      });
    });
  });
}

function finish_test()
{
  gClient.close(function() {
    removeTab(gTab1);
    finish();
  });
}
