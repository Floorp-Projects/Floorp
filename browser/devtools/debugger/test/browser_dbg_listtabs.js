/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gTab1 = null;
var gTab1Actor = null;

var gTab2 = null;
var gTab2Actor = null;

var gClient = null;

function test()
{
  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(function(aType, aTraits) {
    is(aType, "browser", "Root actor should identify itself as a browser.");
    test_first_tab();
  });
}

/**
 * Verify that a new tab shows up in a listTabs call.
 */
function test_first_tab()
{
  gTab1 = addTab(TAB1_URL, function() {
    gClient.listTabs(function(aResponse) {
      for each (let tab in aResponse.tabs) {
        if (tab.url == TAB1_URL) {
          gTab1Actor = tab.actor;
        }
      }
      ok(gTab1Actor, "Should find a tab actor for tab1.");
      test_second_tab();
    });
  });
}

function test_second_tab()
{
  gTab2 = addTab(TAB2_URL, function() {
    gClient.listTabs(function(aResponse) {
      // Verify that tab1 has the same actor it used to.
      let foundTab1 = false;
      for each (let tab in aResponse.tabs) {
        if (tab.url == TAB1_URL) {
          is(tab.actor, gTab1Actor, "Tab1's actor shouldn't have changed.");
          foundTab1 = true;
        }
        if (tab.url == TAB2_URL) {
          gTab2Actor = tab.actor;
        }
      }
      ok(foundTab1, "Should have found an actor for tab 1.");
      ok(gTab2Actor != null, "Should find an actor for tab2.");

      test_remove_tab();
    });
  });
}

function test_remove_tab()
{
  removeTab(gTab1);
  gTab1 = null;
  gClient.listTabs(function(aResponse) {
    // Verify that tab1 is no longer included in listTabs.
    let foundTab1 = false;
    for each (let tab in aResponse.tabs) {
      if (tab.url == TAB1_URL) {
        ok(false, "Tab1 should be gone.");
      }
    }
    ok(!foundTab1, "Tab1 should be gone.");
    test_attach_removed_tab();
  });
}

function test_attach_removed_tab()
{
  removeTab(gTab2);
  gTab2 = null;
  gClient.addListener("paused", function(aEvent, aPacket) {
    ok(false, "Attaching to an exited tab actor shouldn't generate a pause.");
    finish_test();
  });

  gClient.request({ to: gTab2Actor, type: "attach" }, function(aResponse) {
    is(aResponse.error, "noSuchActor", "Tab should be gone.");
    finish_test();
  });
}

function finish_test()
{
  gClient.close(function() {
    finish();
  });
}
