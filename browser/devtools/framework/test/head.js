/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/browser/devtools/framework/test/shared-head.js", this);

function toggleAllTools(state) {
  for (let [, tool] of gDevTools._tools) {
    if (!tool.visibilityswitch) {
      continue;
    }
    if (state) {
      Services.prefs.setBoolPref(tool.visibilityswitch, true);
    } else {
      Services.prefs.clearUserPref(tool.visibilityswitch);
    }
  }
}

function getChromeActors(callback)
{
  let { DebuggerServer } = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
  let { DebuggerClient } = Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
  DebuggerServer.allowChromeProcess = true;

  let client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect(() => {
    client.getProcess().then(response => {
      callback(client, response.form);
    });
  });

  SimpleTest.registerCleanupFunction(() => {
    DebuggerServer.destroy();
  });
}

function getSourceActor(aSources, aURL) {
  let item = aSources.getItemForAttachment(a => a.source.url === aURL);
  return item && item.value;
}

/**
 * Open a Scratchpad window.
 *
 * @return nsIDOMWindow
 *         The new window object that holds Scratchpad.
 */
function *openScratchpadWindow () {
  let { promise: p, resolve } = promise.defer();
  let win = ScratchpadManager.openScratchpad();

  yield once(win, "load");

  win.Scratchpad.addObserver({
    onReady: function () {
      win.Scratchpad.removeObserver(this);
      resolve(win);
    }
  });
  return p;
}
