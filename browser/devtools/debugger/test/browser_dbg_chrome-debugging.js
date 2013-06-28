/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that chrome debugging works.

var gClient = null;
var gTab = null;
var gMozillaTab = null;
var gThreadClient = null;
var gNewGlobal = false;
var gAttached = false;
var gChromeSource = false;

const DEBUGGER_TAB_URL = EXAMPLE_URL + "browser_dbg_debuggerstatement.html";

function test()
{
  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(function(aType, aTraits) {
    gTab = addTab(DEBUGGER_TAB_URL, function() {
      gClient.listTabs(function(aResponse) {
        let dbg = aResponse.chromeDebugger;
        ok(dbg, "Found a chrome debugging actor.");

        gClient.addOneTimeListener("newGlobal", function() gNewGlobal = true);
        gClient.addListener("newSource", onNewSource);

        gClient.attachThread(dbg, function(aResponse, aThreadClient) {
          gThreadClient = aThreadClient;
          ok(!aResponse.error, "Attached to the chrome debugger.");
          gAttached = true;

          // Ensure that a new global will be created.
          gMozillaTab = gBrowser.addTab("about:mozilla");

          finish_test();
        });
      });
    });
  });
}

function onNewSource(aEvent, aPacket)
{
  gChromeSource = aPacket.source.url.startsWith("chrome:");
  finish_test();
}

function finish_test()
{
  if (!gAttached || !gChromeSource) {
    return;
  }
  gClient.removeListener("newSource", onNewSource);
  gThreadClient.resume(function(aResponse) {
    removeTab(gMozillaTab);
    removeTab(gTab);
    gClient.close(function() {
      ok(gNewGlobal, "Received newGlobal event.");
      ok(gChromeSource, "Received newSource event for a chrome: script.");
      finish();
    });
  });
}
