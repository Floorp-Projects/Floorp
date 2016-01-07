/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  gBrowser.selectedTab = gBrowser.addTab();
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  const onLoad = Task.async(function *(evt) {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad);

    const toolbox = yield gDevTools.showToolbox(target, "webconsole");
    ok(toolbox.isReady, "toolbox isReady is set");
    ok(toolbox.threadClient, "toolbox has a thread client");

    const toolbox2 = yield gDevTools.showToolbox(toolbox.target, toolbox.toolId);
    is(toolbox2, toolbox, "same toolbox");

    yield toolbox.destroy();
    gBrowser.removeCurrentTab();
    finish();
  });

  gBrowser.selectedBrowser.addEventListener("load", onLoad, true);
  content.location = "data:text/html,test for toolbox being ready";
}
