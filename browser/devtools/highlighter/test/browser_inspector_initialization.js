/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
let doc;
let salutation;
let closing;
let winId;

function createDocument()
{
  doc.body.innerHTML = '<div id="first" style="{ margin: 10em; ' +
    'font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA}">\n' +
    '<h1>Some header text</h1>\n' +
    '<p id="salutation" style="{font-size: 12pt}">hi.</p>\n' +
    '<p id="body" style="{font-size: 12pt}">I am a test-case. This text exists ' +
    'solely to provide some things to test the inspector initialization.</p>\n' +
    'If you are reading this, you should go do something else instead. Maybe ' +
    'read a book. Or better yet, write some test-cases for another bit of code. ' +
    '<span style="{font-style: italic}">Maybe more inspector test-cases!</span></p>\n' +
    '<p id="closing">end transmission</p>\n' +
    '</div>';
  doc.title = "Inspector Initialization Test";
  startInspectorTests();
}

function startInspectorTests()
{
  ok(InspectorUI, "InspectorUI variable exists");
  Services.obs.addObserver(runInspectorTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.toggleInspectorUI();
}

function runInspectorTests()
{
  Services.obs.removeObserver(runInspectorTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
  Services.obs.addObserver(treePanelTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY, false);

  ok(InspectorUI.toolbar, "we have the toolbar.");
  ok(!InspectorUI.toolbar.hidden, "toolbar is visible");
  ok(InspectorUI.inspecting, "Inspector is inspecting");
  ok(!InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is not open");
  ok(!InspectorUI.sidebar.visible, "Inspector sidebar should not visible.");
  ok(InspectorUI.highlighter, "Highlighter is up");
  InspectorUI.inspectNode(doc.body);
  InspectorUI.stopInspecting();

  InspectorUI.treePanel.open();
}

function treePanelTests()
{
  Services.obs.removeObserver(treePanelTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY);
  ok(InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is open");

  InspectorUI.toggleSidebar();
  ok(InspectorUI.sidebar.visible, "Inspector Sidebar should be open");
  InspectorUI.toggleSidebar();
  ok(!InspectorUI.sidebar.visible, "Inspector Sidebar should be closed");
  InspectorUI.sidebar.show();
  InspectorUI.currentInspector.once("sidebaractivated-computedview",
    stylePanelTests)
  InspectorUI.sidebar.activatePanel("computedview");
}

function stylePanelTests()
{
  ok(InspectorUI.sidebar.visible, "Inspector Sidebar is open");
  is(InspectorUI.sidebar.activePanel, "computedview", "Computed View is open");
  ok(computedViewTree(), "Computed view has a cssHtmlTree");

  InspectorUI.sidebar.activatePanel("ruleview");
  executeSoon(function() {
    ruleViewTests();
  });
}

function ruleViewTests()
{
  Services.obs.addObserver(runContextMenuTest,
      InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);

  is(InspectorUI.sidebar.activePanel, "ruleview", "Rule View is open");
  ok(ruleView(), "InspectorUI has a cssRuleView");

  executeSoon(function() {
    InspectorUI.closeInspectorUI();
  });
}

function runContextMenuTest()
{
  Services.obs.removeObserver(runContextMenuTest, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
  Services.obs.addObserver(inspectNodesFromContextTest, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  salutation = doc.getElementById("salutation");
  ok(salutation, "hello, context menu test!");
  let eventDeets = { type : "contextmenu", button : 2 };
  let contextMenu = document.getElementById("contentAreaContextMenu");
  ok(contextMenu, "we have the context menu");
  let contextInspectMenuItem = document.getElementById("context-inspect");
  ok(contextInspectMenuItem, "we have the inspect context menu item");
  EventUtils.synthesizeMouse(salutation, 2, 2, eventDeets);
  is(contextMenu.state, "showing", "context menu is open");
  is(!contextInspectMenuItem.hidden, gPrefService.getBoolPref("devtools.inspector.enabled"), "is context menu item enabled?");
  contextMenu.hidePopup();
  executeSoon(function() {
    InspectorUI.openInspectorUI(salutation);
  });
}

function inspectNodesFromContextTest()
{
  Services.obs.removeObserver(inspectNodesFromContextTest, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  Services.obs.addObserver(openInspectorForContextTest, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
  ok(!InspectorUI.inspecting, "Inspector is not actively highlighting");
  is(InspectorUI.selection, salutation, "Inspector is highlighting salutation");
  executeSoon(function() {
    InspectorUI.closeInspectorUI(true);
  });
}

function openInspectorForContextTest()
{
  Services.obs.removeObserver(openInspectorForContextTest, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
  Services.obs.addObserver(inspectNodesFromContextTestWhileOpen, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  executeSoon(function() {
    InspectorUI.openInspectorUI(salutation);
  });
}

function inspectNodesFromContextTestWhileOpen()
{
  Services.obs.removeObserver(inspectNodesFromContextTestWhileOpen, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
  Services.obs.addObserver(inspectNodesFromContextTestTrap, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.highlighter.addListener("nodeselected", inspectNodesFromContextTestHighlight);
  is(InspectorUI.selection, salutation, "Inspector is highlighting salutation");
  closing = doc.getElementById("closing");
  ok(closing, "we have the closing statement");
  executeSoon(function() {
    InspectorUI.openInspectorUI(closing);
  });
}

function inspectNodesFromContextTestHighlight()
{
  winId = InspectorUI.winID;
  InspectorUI.highlighter.removeListener("nodeselected", inspectNodesFromContextTestHighlight);
  Services.obs.addObserver(finishInspectorTests, InspectorUI.INSPECTOR_NOTIFICATIONS.DESTROYED, false);
  is(InspectorUI.selection, closing, "InspectorUI.selection is header");
  executeSoon(function() {
    InspectorUI.closeInspectorUI();
  });
}

function inspectNodesFromContextTestTrap()
{
  Services.obs.removeObserver(inspectNodesFromContextTestTrap, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
  ok(false, "Inspector UI has been opened again. We Should Not Be Here!");
}

function finishInspectorTests(subject, topic, aWinIdString)
{
  Services.obs.removeObserver(finishInspectorTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.DESTROYED);

  is(parseInt(aWinIdString), winId, "winId of destroyed Inspector matches");
  ok(!InspectorUI.highlighter, "Highlighter is gone");
  ok(!InspectorUI.treePanel, "Inspector Tree Panel is closed");
  ok(!InspectorUI.inspecting, "Inspector is not inspecting");
  ok(!InspectorUI._sidebar, "Inspector Sidebar is closed");
  ok(!InspectorUI.toolbar, "toolbar is hidden");

  Services.obs.removeObserver(inspectNodesFromContextTestTrap, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
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

