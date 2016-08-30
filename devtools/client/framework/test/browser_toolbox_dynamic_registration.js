/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var toolbox;

function test()
{
  gBrowser.selectedTab = gBrowser.addTab();
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    gDevTools.showToolbox(target).then(testRegister);
  }, true);

  content.location = "data:text/html,test for dynamically registering and unregistering tools";
}

function testRegister(aToolbox)
{
  toolbox = aToolbox;
  gDevTools.once("tool-registered", toolRegistered);

  gDevTools.registerTool({
    id: "test-tool",
    label: "Test Tool",
    inMenu: true,
    isTargetSupported: () => true,
    build: function () {},
    key: "t"
  });
}

function toolRegistered(event, toolId)
{
  is(toolId, "test-tool", "tool-registered event handler sent tool id");

  ok(gDevTools.getToolDefinitionMap().has(toolId), "tool added to map");

  // test that it appeared in the UI
  let doc = toolbox.doc;
  let tab = doc.getElementById("toolbox-tab-" + toolId);
  ok(tab, "new tool's tab exists in toolbox UI");

  let panel = doc.getElementById("toolbox-panel-" + toolId);
  ok(panel, "new tool's panel exists in toolbox UI");

  for (let win of getAllBrowserWindows()) {
    let key = win.document.getElementById("key_" + toolId);
    ok(key, "key for new tool added to every browser window");
    let menuitem = win.document.getElementById("menuitem_" + toolId);
    ok(menuitem, "menu item of new tool added to every browser window");
  }

  // then unregister it
  testUnregister();
}

function getAllBrowserWindows() {
  let wins = [];
  let enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    wins.push(enumerator.getNext());
  }
  return wins;
}

function testUnregister()
{
  gDevTools.once("tool-unregistered", toolUnregistered);

  gDevTools.unregisterTool("test-tool");
}

function toolUnregistered(event, toolDefinition)
{
  let toolId = toolDefinition.id;
  is(toolId, "test-tool", "tool-unregistered event handler sent tool id");

  ok(!gDevTools.getToolDefinitionMap().has(toolId), "tool removed from map");

  // test that it disappeared from the UI
  let doc = toolbox.doc;
  let tab = doc.getElementById("toolbox-tab-" + toolId);
  ok(!tab, "tool's tab was removed from the toolbox UI");

  let panel = doc.getElementById("toolbox-panel-" + toolId);
  ok(!panel, "tool's panel was removed from toolbox UI");

  for (let win of getAllBrowserWindows()) {
    let key = win.document.getElementById("key_" + toolId);
    ok(!key, "key removed from every browser window");
    let menuitem = win.document.getElementById("menuitem_" + toolId);
    ok(!menuitem, "menu item removed from every browser window");
  }

  cleanup();
}

function cleanup()
{
  toolbox.destroy();
  toolbox = null;
  gBrowser.removeCurrentTab();
  finish();
}
