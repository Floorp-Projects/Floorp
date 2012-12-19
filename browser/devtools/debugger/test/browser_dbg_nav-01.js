/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check tab attach/navigation.
 */

var gTab1 = null;
var gTab1Actor = null;

var gClient = null;

function test()
{
  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(function(aType, aTraits) {
    is(aType, "browser", "Root actor should identify itself as a browser.");
    get_tab();
  });
}

function get_tab()
{
  gTab1 = addTab(TAB1_URL, function() {
    get_tab_actor_for_url(gClient, TAB1_URL, function(aGrip) {
      gTab1Actor = aGrip.actor;
      gClient.request({ to: aGrip.actor, type: "attach" }, function(aResponse) {
        gClient.addListener("tabNavigated", function onTabNavigated(aEvent, aPacket) {
          dump("onTabNavigated state " + aPacket.state + "\n");
          if (aPacket.state == "start") {
            return;
          }
          gClient.removeListener("tabNavigated", onTabNavigated);

          is(aPacket.url, TAB2_URL, "Got a tab navigation notification.");
          gClient.addOneTimeListener("tabDetached", function (aEvent, aPacket) {
            ok(true, "Got a tab detach notification.");
            finish_test();
          });
          removeTab(gTab1);
        });
        gTab1.linkedBrowser.loadURI(TAB2_URL);
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
