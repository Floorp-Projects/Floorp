/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let temp = {};

Cu.import("resource://gre/modules/devtools/dbg-server.jsm", temp);
let DebuggerServer = temp.DebuggerServer;

Cu.import("resource://gre/modules/devtools/dbg-client.jsm", temp);
let DebuggerClient = temp.DebuggerClient;
let debuggerSocketConnect = temp.debuggerSocketConnect;

Cu.import("resource:///modules/devtools/profiler/controller.js", temp);
let ProfilerController = temp.ProfilerController;

function test() {
  waitForExplicitFinish();
  Services.prefs.setBoolPref(REMOTE_ENABLED, true);

  loadTab(URL, function onTabLoad(tab, browser) {
    DebuggerServer.init(function () true);
    DebuggerServer.addBrowserActors();
    is(DebuggerServer.listeningSockets, 0);

    DebuggerServer.openListener(2929);
    is(DebuggerServer.listeningSockets, 1);

    let transport = debuggerSocketConnect("127.0.0.1", 2929);
    let client = new DebuggerClient(transport);
    client.connect(function onClientConnect() {
      let target = { isRemote: true, client: client };
      let controller = new ProfilerController(target);

      controller.connect(function onControllerConnect() {
        // If this callback is called, this means listTabs call worked.
        // Which means that the transport worked. Time to finish up this
        // test.

        function onShutdown() {
          window.removeEventListener("Debugger:Shutdown", onShutdown, true);
          transport = client = null;
          finish();
        }

        window.addEventListener("Debugger:Shutdown", onShutdown, true);

        client.close(function () {
          gBrowser.removeTab(tab);
        });
      });
    });
  });
}
