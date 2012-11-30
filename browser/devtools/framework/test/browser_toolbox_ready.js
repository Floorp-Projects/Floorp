/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {};
Cu.import("resource:///modules/devtools/Target.jsm", tempScope);
let TargetFactory = tempScope.TargetFactory;

let toolbox;

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    openToolbox();
  }, true);

  content.location = "data:text/html,test for dynamically registering and unregistering tools";
}

function openToolbox()
{
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.toggleToolboxForTarget(target);

  toolbox = gDevTools.getToolboxForTarget(target);

  ok(!toolbox.isReady, "toolbox isReady isn't set yet");

  try {
    toolbox.selectTool("webconsole");
    ok(false, "Should throw when selectTool() called before toolbox is ready");
  }
  catch(error) {
    is(error.message, "Can't select tool, wait for toolbox 'ready' event")
  }

  toolbox.once("ready", testReady);
}

function testReady()
{
  ok(toolbox.isReady, "toolbox isReady is set");
  cleanup();
}

function cleanup()
{
  toolbox.destroy();
  toolbox = null;
  gBrowser.removeCurrentTab();
  finish();
}
