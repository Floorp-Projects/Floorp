/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that network requests originating from the toolbox don't get recorded in
// the network panel.

add_task(async function() {
  // TODO: This test tries to verify the normal behavior of the netmonitor and
  // therefore needs to avoid the explicit check for tests. Bug 1167188 will
  // allow us to remove this workaround.
  await pushPref("devtools.testing", false);

  let tab = await addTab(URL_ROOT + "doc_viewsource.html");
  let target = TargetFactory.forTab(tab);
  let toolbox = await gDevTools.showToolbox(target, "styleeditor");
  let panel = toolbox.getPanel("styleeditor");

  is(panel.UI.editors.length, 1, "correct number of editors opened");

  let monitor = await toolbox.selectTool("netmonitor");
  let { store } = monitor.panelWin;

  is(store.getState().requests.requests.size, 0, "No network requests appear in the network panel");

  await gDevTools.closeToolbox(target);
  tab = target = toolbox = panel = null;
  gBrowser.removeCurrentTab();
});
