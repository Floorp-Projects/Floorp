/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that getPanelWhenReady returns the correct panel in promise
// resolutions regardless of whether it has opened first.

var toolbox = null;

const URL = "data:text/html;charset=utf8,test for getPanelWhenReady";

add_task(async function() {
  const tab = await addTab(URL);
  const target = await TargetFactory.forTab(tab);
  toolbox = await gDevTools.showToolbox(target);

  const debuggerPanelPromise = toolbox.getPanelWhenReady("jsdebugger");
  await toolbox.selectTool("jsdebugger");
  const debuggerPanel = await debuggerPanelPromise;

  is(
    debuggerPanel,
    toolbox.getPanel("jsdebugger"),
    "The debugger panel from getPanelWhenReady before loading is the actual panel"
  );

  const debuggerPanel2 = await toolbox.getPanelWhenReady("jsdebugger");
  is(
    debuggerPanel2,
    toolbox.getPanel("jsdebugger"),
    "The debugger panel from getPanelWhenReady after loading is the actual panel"
  );

  await cleanup();
});

async function cleanup() {
  await toolbox.destroy();
  gBrowser.removeCurrentTab();
  toolbox = null;
}
