/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;

function createDocument()
{
  doc.body.innerHTML = '<h1>Sidebar state test</h1>';
  doc.title = "Sidebar State Test";

  InspectorUI.openInspectorUI();

  // Open the sidebar and wait for the default view (the rule view) to show.
  InspectorUI.currentInspector.once("sidebaractivated-ruleview", inspectorRuleViewOpened);

  InspectorUI.sidebar.show();
  InspectorUI.sidebar.activatePanel("ruleview");
}

function inspectorRuleViewOpened()
{
  is(InspectorUI.sidebar.activePanel, "ruleview", "Rule View is selected by default");

  // Select the computed view and turn off the inspector.
  InspectorUI.sidebar.activatePanel("computedview");

  Services.obs.addObserver(inspectorClosed,
    InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
  InspectorUI.closeInspectorUI();
}

function inspectorClosed()
{
  // Reopen the inspector, expect the computed view to be loaded.
  Services.obs.removeObserver(inspectorClosed,
    InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);

  Services.obs.addObserver(computedViewPopulated,
    "StyleInspector-populated", false);

  InspectorUI.openInspectorUI();
}

function computedViewPopulated()
{
  Services.obs.removeObserver(computedViewPopulated,
    "StyleInspector-populated");
  is(InspectorUI.sidebar.activePanel, "computedview", "Computed view is selected by default.");

  finishTest();
}


function finishTest()
{
  InspectorUI.closeInspectorUI();
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,basic tests for inspector";
}

