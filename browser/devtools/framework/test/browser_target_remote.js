/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let { DebuggerServer } =
  Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
let { DebuggerClient } =
  Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});
let { devtools } =
  Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

// Ensure target is closed if client is closed directly
function test() {
  waitForExplicitFinish();

  if (!DebuggerServer.initialized) {
    DebuggerServer.init(function () { return true; });
    DebuggerServer.addBrowserActors();
  }

  var client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect(() => {
    client.listTabs(response => {
      let options = {
        form: response,
        client: client,
        chrome: true
      };

      devtools.TargetFactory.forRemoteTab(options).then(target => {
        target.on("close", () => {
          ok(true, "Target was closed");
          DebuggerServer.destroy();
          finish();
        });
        client.close();
      });
    });
  });
}
