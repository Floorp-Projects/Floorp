/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


let div;
let tab1;
let tab2;
let tab1window;

function inspectorTabOpen1()
{
  ok(window.InspectorUI, "InspectorUI variable exists");
  ok(!InspectorUI.inspecting, "Inspector is not highlighting");
  ok(InspectorUI.store.isEmpty(), "Inspector.store is empty");

  Services.obs.addObserver(inspectorUIOpen1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.openInspectorUI();
}

function inspectorUIOpen1()
{
  Services.obs.removeObserver(inspectorUIOpen1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  // Make sure the inspector is open.
  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(!InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is not open");
  ok(!InspectorUI.isSidebarOpen, "Inspector Sidebar is not open");
  ok(!InspectorUI.store.isEmpty(), "InspectorUI.store is not empty");
  is(InspectorUI.store.length, 1, "Inspector.store.length = 1");

  // Highlight a node.
  div = content.document.getElementsByTagName("div")[0];
  InspectorUI.inspectNode(div);
  is(InspectorUI.selection, div, "selection matches the div element");

  Services.obs.addObserver(inspectorRuleViewOpened,
    InspectorUI.INSPECTOR_NOTIFICATIONS.RULEVIEWREADY, false);

  InspectorUI.showSidebar();
  InspectorUI.openRuleView();
}

function inspectorRuleViewOpened() {
  Services.obs.removeObserver(inspectorRuleViewOpened,
    InspectorUI.INSPECTOR_NOTIFICATIONS.RULEVIEWREADY);

  // Open the second tab.
  tab2 = gBrowser.addTab();
  gBrowser.selectedTab = tab2;

  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee,
      true);
    waitForFocus(inspectorTabOpen2, content);
  }, true);

  content.location = "data:text/html,<p>tab 2: the inspector should close now";
}

function inspectorTabOpen2()
{
  // Make sure the inspector is closed.
  ok(!InspectorUI.inspecting, "Inspector is not highlighting");
  ok(!InspectorUI.treePanel, "Inspector Tree Panel is closed");
  ok(!InspectorUI.isSidebarOpen, "Inspector Sidebar is not open");
  is(InspectorUI.store.length, 1, "Inspector.store.length = 1");

  Services.obs.addObserver(inspectorFocusTab1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.RULEVIEWREADY, false);
  // Switch back to tab 1.
  executeSoon(function() {
    gBrowser.selectedTab = tab1;
  });
}

function inspectorFocusTab1()
{
  Services.obs.removeObserver(inspectorFocusTab1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.RULEVIEWREADY, false);
  Services.obs.addObserver(inspectorRuleTrap,
    InspectorUI.INSPECTOR_NOTIFICATIONS.RULEVIEWREADY, false);

  // Make sure the inspector is open.
  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(!InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is not open");
  is(InspectorUI.store.length, 1, "Inspector.store.length = 1");
  is(InspectorUI.selection, div, "selection matches the div element");
  ok(InspectorUI.isSidebarOpen, "sidebar is open");
  ok(InspectorUI.isRuleViewOpen(), "rule view is open");
  is(InspectorUI.ruleView.doc.documentElement.children.length, 1, "RuleView elements.length == 1");

  requestLongerTimeout(4);
  executeSoon(function() {
    InspectorUI.closeInspectorUI();
    gBrowser.removeCurrentTab(); // tab 1
    gBrowser.removeCurrentTab(); // tab 2
    finish();
  });
}

function inspectorRuleTrap()
{
  Services.obs.removeObserver(inspectorRuleTrap,
    InspectorUI.INSPECTOR_NOTIFICATIONS.RULEVIEWREADY, false);
  is(InspectorUI.ruleView.doc.documentElement.children.length, 1, "RuleView elements.length == 1");
}

function test()
{
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();

  tab1 = gBrowser.addTab();
  gBrowser.selectedTab = tab1;
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee,
      true);
    waitForFocus(inspectorTabOpen1, content);
  }, true);

  content.location = "data:text/html,<p>tab switching tests for inspector" +
    "<div>tab 1</div>";
}

