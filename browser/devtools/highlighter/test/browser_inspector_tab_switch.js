/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  ok(!InspectorUI.sidebar.visible, "Inspector Sidebar is not open");
  ok(!InspectorUI.store.isEmpty(), "InspectorUI.store is not empty");
  is(InspectorUI.store.length, 1, "Inspector.store.length = 1");

  // Highlight a node.
  div = content.document.getElementsByTagName("div")[0];
  InspectorUI.inspectNode(div);
  is(InspectorUI.selection, div, "selection matches the div element");

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
  is(InspectorUI.store.length, 1, "Inspector.store.length = 1");

  // Activate the inspector again.
  executeSoon(function() {
    Services.obs.addObserver(inspectorUIOpen2,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    clearUserPrefs();
    InspectorUI.openInspectorUI();
  });
}

function inspectorUIOpen2()
{
  Services.obs.removeObserver(inspectorUIOpen2,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  // Make sure the inspector is open.
  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(!InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is not open");
  is(InspectorUI.store.length, 2, "Inspector.store.length = 2");

  // Disable highlighting.
  InspectorUI.toggleInspection();
  ok(!InspectorUI.inspecting, "Inspector is not highlighting");


  // Switch back to tab 1.
  executeSoon(function() {
    Services.obs.addObserver(inspectorFocusTab1,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    gBrowser.selectedTab = tab1;
  });
}

function inspectorFocusTab1()
{
  Services.obs.removeObserver(inspectorFocusTab1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  // Make sure the inspector is still open.
  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(!InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is not open");
  is(InspectorUI.store.length, 2, "Inspector.store.length = 2");
  is(InspectorUI.selection, div, "selection matches the div element");

  Services.obs.addObserver(inspectorOpenTreePanelTab1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY, false);

  InspectorUI.toggleHTMLPanel();
}

function inspectorOpenTreePanelTab1()
{
  Services.obs.removeObserver(inspectorOpenTreePanelTab1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY);

  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is open");
  is(InspectorUI.store.length, 2, "Inspector.store.length = 2");
  is(InspectorUI.selection, div, "selection matches the div element");

  InspectorUI.currentInspector.once("sidebaractivated-computedview",
    inspectorSidebarStyleView1);

  executeSoon(function() {
    InspectorUI.sidebar.show();
    InspectorUI.sidebar.activatePanel("computedview");
  });
}

function inspectorSidebarStyleView1()
{
  ok(InspectorUI.sidebar.visible, "Inspector Sidebar is open");
  ok(computedView(), "Inspector Has a computed view Instance");

  InspectorUI.sidebar._toolObjects().forEach(function (aTool) {
    let btn = aTool.button;
    is(btn.hasAttribute("checked"),
      (aTool.id == "computedview"),
      "Button " + btn.label + " has correct checked attribute");
  });

  // Switch back to tab 2.
  Services.obs.addObserver(inspectorFocusTab2,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  gBrowser.selectedTab = tab2;
}

function inspectorFocusTab2()
{
  Services.obs.removeObserver(inspectorFocusTab2,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  // Make sure the inspector is still open.
  ok(!InspectorUI.inspecting, "Inspector is not highlighting");
  ok(!InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is not open");
  ok(!InspectorUI.sidebar.visible, "Inspector Sidebar is not open");
  is(InspectorUI.store.length, 2, "Inspector.store.length is 2");
  isnot(InspectorUI.selection, div, "selection does not match the div element");


  executeSoon(function() {
    // Make sure keybindings still work
    synthesizeKeyFromKeyTag("key_inspect");

    ok(InspectorUI.inspecting, "Inspector is highlighting");
    InspectorUI.toggleInspection();

    // Switch back to tab 1.
    Services.obs.addObserver(inspectorSecondFocusTab1,
      InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY, false);
    gBrowser.selectedTab = tab1;
  });
}

function inspectorSecondFocusTab1()
{
  Services.obs.removeObserver(inspectorSecondFocusTab1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY);

  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is open");
  is(InspectorUI.store.length, 2, "Inspector.store.length = 2");
  is(InspectorUI.selection, div, "selection matches the div element");

  ok(InspectorUI.sidebar.visible, "Inspector Sidebar is open");
  ok(computedView(), "Inspector Has a Style Panel Instance");
  InspectorUI.sidebar._toolObjects().forEach(function(aTool) {
    let btn = aTool.button;
    is(btn.hasAttribute("checked"),
      (aTool.id == "computedview"),
      "Button " + btn.label + " has correct checked attribute");
  });

  // Switch back to tab 2.
  Services.obs.addObserver(inspectorSecondFocusTab2,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  gBrowser.selectedTab = tab2;
}

function inspectorSecondFocusTab2()
{
  Services.obs.removeObserver(inspectorSecondFocusTab2,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);

  // Make sure the inspector is still open.
  ok(!InspectorUI.inspecting, "Inspector is not highlighting");
  ok(!InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is not open");
  ok(!InspectorUI.isSidebarOpen, "Inspector Sidebar is not open");

  is(InspectorUI.store.length, 2, "Inspector.store.length is 2");
  isnot(InspectorUI.selection, div, "selection does not match the div element");

  // Remove tab 1.
  tab1window = gBrowser.getBrowserForTab(tab1).contentWindow;
  tab1window.addEventListener("pagehide", inspectorTabUnload1, false);
  gBrowser.removeTab(tab1);
}

function inspectorTabUnload1(evt)
{
  tab1window.removeEventListener(evt.type, arguments.callee, false);
  tab1window = tab1 = tab2 = div = null;

  // Make sure the Inspector is still open and that the state is correct.
  ok(!InspectorUI.inspecting, "Inspector is not highlighting");
  ok(!InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is not open");
  is(InspectorUI.store.length, 1, "Inspector.store.length = 1");

  InspectorUI.closeInspectorUI();
  gBrowser.removeCurrentTab();
  finish();
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

