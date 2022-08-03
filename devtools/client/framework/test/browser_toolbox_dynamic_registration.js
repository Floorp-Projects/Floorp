/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL =
  "data:text/html,test for dynamically registering and unregistering tools";

var toolbox;

function test() {
  addTab(TEST_URL).then(async tab => {
    gDevTools.showToolboxForTab(tab).then(testRegister);
  });
}

function testRegister(aToolbox) {
  toolbox = aToolbox;
  gDevTools.once("tool-registered", toolRegistered);

  gDevTools.registerTool({
    id: "testTool",
    label: "Test Tool",
    inMenu: true,
    isToolSupported: () => true,
    build() {},
  });
}

function toolRegistered(toolId) {
  is(toolId, "testTool", "tool-registered event handler sent tool id");

  ok(gDevTools.getToolDefinitionMap().has(toolId), "tool added to map");

  // test that it appeared in the UI
  const doc = toolbox.doc;
  const tab = getToolboxTab(doc, toolId);
  ok(tab, "new tool's tab exists in toolbox UI");

  const panel = doc.getElementById("toolbox-panel-" + toolId);
  ok(panel, "new tool's panel exists in toolbox UI");

  for (const win of getAllBrowserWindows()) {
    const menuitem = win.document.getElementById("menuitem_" + toolId);
    ok(menuitem, "menu item of new tool added to every browser window");
  }

  // then unregister it
  testUnregister();
}

function getAllBrowserWindows() {
  return Array.from(Services.wm.getEnumerator("navigator:browser"));
}

function testUnregister() {
  gDevTools.once("tool-unregistered", toolUnregistered);

  gDevTools.unregisterTool("testTool");
}

function toolUnregistered(toolId) {
  is(toolId, "testTool", "tool-unregistered event handler sent tool id");

  ok(!gDevTools.getToolDefinitionMap().has(toolId), "tool removed from map");

  // test that it disappeared from the UI
  const doc = toolbox.doc;
  const tab = getToolboxTab(doc, toolId);
  ok(!tab, "tool's tab was removed from the toolbox UI");

  const panel = doc.getElementById("toolbox-panel-" + toolId);
  ok(!panel, "tool's panel was removed from toolbox UI");

  for (const win of getAllBrowserWindows()) {
    const menuitem = win.document.getElementById("menuitem_" + toolId);
    ok(!menuitem, "menu item removed from every browser window");
  }

  cleanup();
}

function cleanup() {
  toolbox.destroy().then(() => {
    toolbox = null;
    gBrowser.removeCurrentTab();
    finish();
  });
}
