/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "data:text/html,test for toolbox being ready";

add_task(async function () {
  const tab = await addTab(TEST_URL);

  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "webconsole",
  });
  ok(toolbox.isReady, "toolbox isReady is set");
  ok(toolbox.threadFront, "toolbox has a thread front");

  const toolbox2 = await gDevTools.showToolboxForTab(tab, {
    toolId: toolbox.toolId,
  });
  is(toolbox2, toolbox, "same toolbox");

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});
