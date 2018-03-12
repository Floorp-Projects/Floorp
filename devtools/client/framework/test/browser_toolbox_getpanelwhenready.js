/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that getPanelWhenReady returns the correct panel in promise
// resolutions regardless of whether it has opened first.

var toolbox = null;

const URL = "data:text/html;charset=utf8,test for getPanelWhenReady";

add_task(function* () {
  let tab = yield addTab(URL);
  let target = TargetFactory.forTab(tab);
  toolbox = yield gDevTools.showToolbox(target);

  let debuggerPanelPromise = toolbox.getPanelWhenReady("jsdebugger");
  yield toolbox.selectTool("jsdebugger");
  let debuggerPanel = yield debuggerPanelPromise;

  is(debuggerPanel, toolbox.getPanel("jsdebugger"),
      "The debugger panel from getPanelWhenReady before loading is the actual panel");

  let debuggerPanel2 = yield toolbox.getPanelWhenReady("jsdebugger");
  is(debuggerPanel2, toolbox.getPanel("jsdebugger"),
      "The debugger panel from getPanelWhenReady after loading is the actual panel");

  yield cleanup();
});

function* cleanup() {
  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
  toolbox = null;
}
