/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


function test()
{
  let toolbox;
  let inspector;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(startInspector, content);
  }, true);
  content.location = "data:text/html,mop"

  function startInspector() {
    info("Tab loaded");
    openInspector(function(aInspector) {
      inspector = aInspector;
      ok(!inspector.highlighter.hidden, "Highlighter is visible");
      toolbox = inspector._toolbox;
      toolbox.once("webconsole-selected", onWebConsoleSelected);
      toolbox.selectTool("webconsole");
    });
  }

  function onWebConsoleSelected() {
    executeSoon(function() {
      ok(inspector.highlighter.hidden, "Highlighter is hidden");
      toolbox.once("inspector-selected", onInspectorSelected);
      toolbox.selectTool("inspector");
    });
  }

  function onInspectorSelected() {
    executeSoon(function() {
      ok(!inspector.highlighter.hidden, "Highlighter is visible once inspector reopen");
      gBrowser.removeCurrentTab();
      finish();
    });
  }
}

