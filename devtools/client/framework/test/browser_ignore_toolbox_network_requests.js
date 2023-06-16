/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that network requests originating from the toolbox don't get recorded in
// the network panel.

add_task(async function () {
  let tab = await addTab(URL_ROOT + "doc_viewsource.html");
  let toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "styleeditor",
  });
  let panel = toolbox.getPanel("styleeditor");

  is(panel.UI.editors.length, 1, "correct number of editors opened");

  const monitor = await toolbox.selectTool("netmonitor");
  const { store } = monitor.panelWin;

  is(
    store.getState().requests.requests.length,
    0,
    "No network requests appear in the network panel"
  );

  await toolbox.destroy();
  tab = toolbox = panel = null;
  gBrowser.removeCurrentTab();
});
